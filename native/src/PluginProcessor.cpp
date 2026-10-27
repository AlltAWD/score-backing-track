// SPDX-License-Identifier: GPL-3.0-only
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

juce::AudioProcessorValueTreeState::ParameterLayout BackingProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "offset", 1 },
        "Recording offset", juce::NormalisableRange<float>(-600.0f, 600.0f, 0.001f), 0.0f));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "gain", 1 },
        "Recording volume", juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), -6.0f));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "mute", 1 },
        "Mute recording", false));
    return result;
}

BackingProcessor::BackingProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "MuseBackingState", makeParameters()),
      offsetParameter(parameters.getRawParameterValue("offset")),
      gainParameter(parameters.getRawParameterValue("gain")),
      muteParameter(parameters.getRawParameterValue("mute"))
{
    formats.registerBasicFormats();
}

BackingProcessor::~BackingProcessor()
{
    ++loadGeneration;
    loader.removeAllJobs(true, -1);
}

void BackingProcessor::prepareToPlay(double rate, int)
{
    engineSampleRate = rate > 0 ? rate : 44100;
    gain.reset(engineSampleRate, 0.02);
    gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(gainParameter->load()));
    previouslyPlaying = false;
    lastReportedSeconds = -1;
}

bool BackingProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto& output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && output == layout.getMainInputChannelSet();
}

void BackingProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto* head = getPlayHead();
    const auto position = head != nullptr ? head->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    const bool hasTime = position.hasValue()
        && (position->getTimeInSamples().hasValue() || position->getTimeInSeconds().hasValue());
    hostAvailable.store(hasTime);
    const bool playing = hasTime && position->getIsPlaying();
    hostPlaying.store(playing);
    if (!hasTime) { previouslyPlaying = false; return; }

    const double reportedSeconds = position->getTimeInSamples().hasValue()
        ? static_cast<double>(*position->getTimeInSamples()) / engineSampleRate
        : *position->getTimeInSeconds();
    double seconds = reportedSeconds;
    // MuseScore 4.7's offline master mixer reports zero for every block, even
    // with the realtime process flag. Advance repeated timestamps while playing;
    // any changed timestamp or pause immediately reanchors to the host.
    if (playing && previouslyPlaying && reportedSeconds == lastReportedSeconds)
        seconds = nextBlockSeconds;
    nextBlockSeconds = seconds + buffer.getNumSamples() / engineSampleRate;
    lastReportedSeconds = reportedSeconds;
    previouslyPlaying = playing;
    hostSeconds.store(seconds);
    const double offset = static_cast<double>(offsetParameter->load());
    recordingSeconds.store(seconds + offset);
    if (!playing || !std::isfinite(seconds) || !std::isfinite(offset)) return;

    // The audio thread never waits for loading or frees a decoded recording.
    // A swap may skip one block; decoding and deleting old clips happen off-thread.
    const juce::SpinLock::ScopedTryLockType lock(clipLock);
    if (!lock.isLocked() || clip == nullptr) return;
    const auto& audio = clip->audio;
    const int frames = audio.getNumSamples();
    const double step = clip->sampleRate / engineSampleRate;
    double source = (seconds + offset) * clip->sampleRate;
    gain.setTargetValue(muteParameter->load() >= 0.5f ? 0.0f
        : juce::Decibels::decibelsToGain(gainParameter->load()));

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample, source += step)
    {
        const float level = gain.getNextValue();
        if (source < 0 || source >= frames) continue;
        const int first = static_cast<int>(source);
        const int second = juce::jmin(first + 1, frames - 1);
        const float mix = static_cast<float>(source - first);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* input = audio.getReadPointer(juce::jmin(channel, audio.getNumChannels() - 1));
            const float value = input[first] + mix * (input[second] - input[first]);
            buffer.addSample(channel, sample, value * level);
        }
    }
}

void BackingProcessor::loadFile(const juce::File& file, bool notifyHost)
{
    const auto generation = ++loadGeneration;
    {
        const juce::ScopedLock lock(infoLock);
        error.clear();
        requestedPath = file.getFullPathName();
        loading.store(true);
    }
    loader.addJob([this, file, generation, notifyHost] { decode(file, generation, notifyHost); });
}

void BackingProcessor::decode(const juce::File& file, uint64_t generation, bool notifyHost)
{
    if (generation != loadGeneration.load()) return;
    if (!file.existsAsFile()) { setError("Recording not found. Choose its new location.", generation); return; }
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr) { setError("Cannot read this recording. Try WAV, MP3, FLAC, OGG or AIFF.", generation); return; }
    const int channels = juce::jmin(2, static_cast<int>(reader->numChannels));
    constexpr int64_t maximumBytes = 512LL * 1024 * 1024;
    if (channels < 1 || reader->sampleRate <= 0 || !std::isfinite(reader->sampleRate)
        || reader->lengthInSamples <= 0
        || reader->lengthInSamples > maximumBytes / (channels * static_cast<int>(sizeof(float)))
        || reader->lengthInSamples > std::numeric_limits<int>::max())
    { setError("Recording is empty or exceeds the 512 MB decoded-audio limit.", generation); return; }
    try
    {
        auto loaded = std::make_unique<Clip>();
        loaded->audio.setSize(channels, static_cast<int>(reader->lengthInSamples));
        loaded->sampleRate = reader->sampleRate;
        const auto path = file.getFullPathName();
        const double duration = loaded->audio.getNumSamples() / loaded->sampleRate;
        constexpr int chunk = 65536;
        for (int start = 0; start < loaded->audio.getNumSamples(); start += chunk)
        {
            if (generation != loadGeneration.load()) return;
            const int size = juce::jmin(chunk, loaded->audio.getNumSamples() - start);
            if (!reader->read(&loaded->audio, start, size, start, true, channels > 1))
            { setError("The recording could not be decoded completely.", generation); return; }
        }
        // Sanitize malformed floating-point audio before it reaches the host.
        for (int channel = 0; channel < channels; ++channel)
            for (int i = 0; i < loaded->audio.getNumSamples(); ++i)
                if (!std::isfinite(loaded->audio.getSample(channel, i))) loaded->audio.setSample(channel, i, 0);
        if (generation != loadGeneration.load()) return;
        {
            // Publish the clip and its metadata together (lock order: info, then clip).
            const juce::ScopedLock info(infoLock);
            if (generation != loadGeneration.load()) return;
            {
                const juce::SpinLock::ScopedLockType lock(clipLock);
                clip.swap(loaded);
            }
            loadedPath = path;
            loadedDuration = duration;
            loading.store(false);
        }
        // 'loaded' now owns the previous recording; destroy it on this worker.
        loaded.reset();
        if (notifyHost) updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
    }
    catch (const std::bad_alloc&)
    { setError("Not enough memory to load this recording.", generation); }
}

void BackingProcessor::setError(juce::String text, uint64_t generation)
{
    if (generation != loadGeneration.load()) return;
    const juce::ScopedLock lock(infoLock);
    if (generation != loadGeneration.load()) return;
    error = std::move(text);
    loading.store(false);
}

void BackingProcessor::clearFile()
{
    ++loadGeneration;
    std::unique_ptr<Clip> previous;
    {
        const juce::SpinLock::ScopedLockType lock(clipLock);
        clip.swap(previous);
    }
    {
        const juce::ScopedLock lock(infoLock);
        error.clear();
        requestedPath.clear();
        loadedPath.clear();
        loadedDuration = 0;
        loading.store(false);
    }
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
}

juce::String BackingProcessor::getError() const
{
    const juce::ScopedLock lock(infoLock);
    return error;
}

juce::String BackingProcessor::getFilePath() const
{
    const juce::ScopedLock lock(infoLock);
    return loadedPath;
}

juce::String BackingProcessor::getRequestedPath() const
{
    const juce::ScopedLock lock(infoLock);
    return requestedPath;
}

double BackingProcessor::getDuration() const
{
    const juce::ScopedLock lock(infoLock);
    return loadedDuration;
}

bool BackingProcessor::hasClip() const
{
    const juce::ScopedLock lock(infoLock);
    return loadedPath.isNotEmpty();
}

void BackingProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    juce::String path;
    {
        const juce::ScopedLock lock(infoLock);
        path = loading.load() || loadedPath.isEmpty() ? requestedPath : loadedPath;
    }
    state.setProperty("audioFile", path, nullptr);
    if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void BackingProcessor::setStateInformation(const void* data, int size)
{
    const auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
    auto state = juce::ValueTree::fromXml(*xml);
    const auto path = state.getProperty("audioFile").toString();
    parameters.replaceState(state);
    if (path.isEmpty()) clearFile();
    else loadFile(juce::File(path), false);
}

juce::AudioProcessorEditor* BackingProcessor::createEditor() { return new BackingEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BackingProcessor(); }
