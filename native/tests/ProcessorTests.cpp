// SPDX-License-Identifier: GPL-3.0-only
#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>

namespace
{
int assertions = 0;
void require(bool value, const char* message)
{
    ++assertions;
    if (!value) throw std::runtime_error(message);
}
void near(float actual, float expected, const char* message, float tolerance = 0.00015f)
{
    const auto detail = juce::String(message) + " (actual " + juce::String(actual, 6)
        + ", expected " + juce::String(expected, 6) + ")";
    require(std::abs(actual - expected) < tolerance, detail.toRawUTF8());
}

class Head final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        if (!available) return {};
        PositionInfo position;
        position.setIsPlaying(playing);
        if (samplesAvailable) position.setTimeInSamples(samples);
        else position.setTimeInSeconds(seconds);
        return position;
    }
    bool available = true, playing = true, samplesAvailable = true;
    int64_t samples = 0;
    double seconds = 0;
};

void parameter(BackingProcessor& processor, const char* id, float value)
{
    auto* p = processor.parameters.getParameter(id);
    p->setValueNotifyingHost(p->convertTo0to1(value));
}

void wait(BackingProcessor& processor)
{
    for (int i = 0; i < 500 && processor.isLoading(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(!processor.isLoading(), "Decoder timed out");
}

float waveform(int sample) { return static_cast<float>((sample % 1000) - 500) / 2000.0f; }

juce::File makeWave()
{
    auto file = juce::File::createTempFile(".wav");
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 1, 24, {}, 0));
    require(writer != nullptr, "Could not create WAV fixture");
    juce::AudioBuffer<float> audio(1, 24000);
    for (int i = 0; i < audio.getNumSamples(); ++i) audio.setSample(0, i, waveform(i));
    require(writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()), "Could not write WAV fixture");
    return file;
}

void process(BackingProcessor& processor, juce::AudioBuffer<float>& buffer, float input = 0)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), input, buffer.getNumSamples());
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}

void run(const juce::File& wav)
{
    BackingProcessor processor;
    Head head;
    processor.setPlayHead(&head);
    // These checks compare exact samples right after seeks; runFade covers the fade.
    processor.setSeekFadeSeconds(0);
    parameter(processor, "gain", 0);
    processor.prepareToPlay(48000, 2048);
    juce::AudioBuffer<float> block(2, 128);
    std::cout << "  raw defaults: speed=" << juce::String(processor.parameters.getRawParameterValue("speed")->load(), 9)
              << " offset=" << juce::String(processor.parameters.getRawParameterValue("offset")->load(), 9) << '\n';

    // A generator effect must preserve the existing score mix, even without audio.
    process(processor, block, 0.1f);
    near(block.getSample(0, 64), 0.1f, "Empty plugin altered score mix");
    processor.loadFile(wav);
    wait(processor);
    require(processor.hasClip() && processor.getError().isEmpty(), "WAV did not load");
    require(std::abs(processor.getDuration() - 0.5) < 0.000001, "WAV duration wrong");

    // Absolute host sample positions, with passthrough mix and mono duplication.
    head.samples = 4371;
    process(processor, block, 0.1f);
    near(block.getSample(0, 0), 0.1f + waveform(4371), "Selected start wrong");
    near(block.getSample(1, 127), 0.1f + waveform(4498), "Mono duplication/end of block wrong");
    head.samples = 100;
    process(processor, block);
    near(block.getSample(0, 0), waveform(100), "Backward seek/loop retained old cursor");
    head.playing = false;
    process(processor, block, 0.1f);
    near(block.getSample(0, 0), 0.1f, "Paused plugin emitted recording");
    head.playing = true;
    head.samples = 8000;
    process(processor, block);
    near(block.getSample(0, 0), waveform(8000), "Resume did not follow host");
    processor.setNonRealtime(true);
    head.samples = 0;
    process(processor, block);
    process(processor, block);
    near(block.getSample(0, 0), waveform(128), "Frozen offline host timestamp repeated first block");
    processor.setNonRealtime(false);
    head.available = false;
    process(processor, block, 0.15f);
    near(block.getSample(0, 1), 0.15f, "Missing host time continued audio");
    require(!processor.hostAvailable.load(), "Missing host time not reported");
    head.available = true;

    parameter(processor, "offset", 0.125f);
    head.samples = 100;
    process(processor, block);
    near(block.getSample(0, 0), waveform(6100), "Positive offset wrong");
    parameter(processor, "offset", -0.125f);
    process(processor, block);
    near(block.getSample(0, 0), 0, "Negative offset played before start");
    head.samples = 6100;
    process(processor, block);
    near(block.getSample(0, 0), waveform(100), "Negative offset did not enter recording correctly");
    parameter(processor, "offset", 0);
    head.samples = 23950;
    process(processor, block);
    near(block.getSample(0, 49), waveform(23999), "End of recording truncated early");
    near(block.getSample(0, 50), 0, "End of recording read out of range");

    // Different host/device rate: fractional source position, linear resampling.
    processor.prepareToPlay(44100, 2048);
    head.samples = 200;
    process(processor, block);
    const double source = 200.0 * 48000.0 / 44100.0;
    const int index = static_cast<int>(source);
    const float fraction = static_cast<float>(source - index);
    near(block.getSample(0, 0), waveform(index) + fraction * (waveform(index + 1) - waveform(index)), "Sample-rate conversion wrong");
    head.samplesAvailable = false;
    head.seconds = 0.2;
    process(processor, block);
    near(block.getSample(0, 0), waveform(9600), "Host time-in-seconds fallback wrong");
    head.samplesAvailable = true;
    processor.prepareToPlay(48000, 2048);

    // Mute fades over 20 ms without changing the incoming score.
    parameter(processor, "mute", 1);
    juce::AudioBuffer<float> large(2, 2048);
    head.samples = 1000;
    process(processor, large, 0.1f);
    near(large.getSample(0, 1800), 0.1f, "Mute did not silence recording");
    parameter(processor, "mute", 0);
    parameter(processor, "gain", -6);
    parameter(processor, "offset", 0.125f);

    // The score's plugin state carries the recording path and all parameters.
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    BackingProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    wait(restored);
    require(restored.getFilePath() == wav.getFullPathName(), "Recording path did not restore");
    near(restored.parameters.getRawParameterValue("offset")->load(), 0.125f, "Offset did not restore");
    near(restored.parameters.getRawParameterValue("gain")->load(), -6, "Gain did not restore");
    const char bad[] = "invalid preset";
    restored.setStateInformation(bad, sizeof(bad));
    require(restored.hasClip(), "Invalid state destroyed current recording");

    processor.loadFile(wav.getSiblingFile("missing-recording.wav"));
    wait(processor);
    require(processor.getError().isNotEmpty() && processor.hasClip(), "Missing file did not preserve last valid recording");
    processor.clearFile();
    require(!processor.hasClip() && processor.getError().isEmpty(), "Remove did not clear recording/error");

    // A score whose recording moved restores its path, reports it, and saves it again.
    BackingProcessor moved;
    const auto missing = wav.getSiblingFile("moved-recording.wav");
    parameter(moved, "offset", 0.25f);
    juce::MemoryBlock movedState;
    moved.getStateInformation(movedState);
    auto movedXml = moved.getXmlFromBinary(movedState.getData(), static_cast<int>(movedState.getSize()));
    require(movedXml != nullptr, "Could not read saved state");
    movedXml->setAttribute("audioFile", missing.getFullPathName());
    moved.copyXmlToBinary(*movedXml, movedState);
    moved.setStateInformation(movedState.getData(), static_cast<int>(movedState.getSize()));
    wait(moved);
    require(!moved.hasClip() && moved.getError().isNotEmpty(), "Missing restored recording not reported");
    require(moved.getRequestedPath() == missing.getFullPathName(), "Missing recording path not shown");
    juce::MemoryBlock resaved;
    moved.getStateInformation(resaved);
    const auto resavedXml = moved.getXmlFromBinary(resaved.getData(), static_cast<int>(resaved.getSize()));
    require(resavedXml != nullptr && resavedXml->getStringAttribute("audioFile") == missing.getFullPathName(),
        "Saving lost the missing recording's path");
    near(moved.parameters.getRawParameterValue("offset")->load(), 0.25f, "Offset lost with missing recording");

    // Repeated replacements and cancellation must not publish a stale decode.
    for (int i = 0; i < 10; ++i) processor.loadFile(wav);
    processor.clearFile();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    require(!processor.hasClip(), "Cancelled decode published stale recording");
}

void runFade(const juce::File& wav)
{
    BackingProcessor processor;
    Head head;
    processor.setPlayHead(&head);
    parameter(processor, "gain", 0);
    processor.prepareToPlay(48000, 2048);
    processor.loadFile(wav);
    wait(processor);
    juce::AudioBuffer<float> block(2, 480);

    // A jump starts silent and reaches full level within 5 ms (240 samples).
    head.samples = 4371;
    process(processor, block);
    near(block.getSample(0, 0), waveform(4371) / 240.0f, "Seek did not fade in");
    near(block.getSample(0, 300), waveform(4671), "Fade-in did not finish within 5 ms");
    head.samples += 480;
    process(processor, block);
    near(block.getSample(0, 0), waveform(4851), "Continuous playback was faded");
    parameter(processor, "offset", 0.01f);
    head.samples += 480;
    process(processor, block);
    near(block.getSample(0, 0), waveform(4851 + 480 + 480) / 240.0f, "Offset nudge did not fade in");
}

// 2 s at 48 kHz, mono: silence, a 440 Hz tone from 0.5 s to 1.5 s, silence.
juce::File makeTone()
{
    auto file = juce::File::createTempFile(".wav");
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 1, 24, {}, 0));
    require(writer != nullptr, "Could not create tone fixture");
    juce::AudioBuffer<float> audio(1, 96000);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const double time = i / 48000.0;
        audio.setSample(0, i, time >= 0.5 && time < 1.5
            ? 0.5f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 440.0 * time)) : 0.0f);
    }
    require(writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()), "Could not write tone fixture");
    return file;
}

// Plays from a host position for a number of seconds; returns the left channel.
std::vector<float> render(BackingProcessor& processor, Head& head, double startSeconds, double seconds, int blockSize)
{
    std::vector<float> out;
    juce::AudioBuffer<float> block(2, blockSize);
    head.samples = static_cast<int64_t>(startSeconds * 48000);
    while (out.size() < seconds * 48000)
    {
        process(processor, block);
        out.insert(out.end(), block.getReadPointer(0), block.getReadPointer(0) + blockSize);
        head.samples += blockSize;
    }
    return out;
}

double onsetSeconds(const std::vector<float>& signal, double startSeconds)
{
    for (size_t i = 0; i < signal.size(); ++i)
        if (std::abs(signal[i]) > 0.1f) return startSeconds + i / 48000.0;
    return -1;
}

double frequency(const std::vector<float>& signal, double startSeconds, double from, double to)
{
    int crossings = 0;
    const auto first = static_cast<size_t>((from - startSeconds) * 48000);
    const auto last = static_cast<size_t>((to - startSeconds) * 48000);
    for (size_t i = first + 1; i < last; ++i)
        if (signal[i - 1] < 0 && signal[i] >= 0) ++crossings;
    return crossings / (to - from);
}

void runSpeed(const juce::File& tone)
{
    BackingProcessor processor;
    Head head;
    processor.setPlayHead(&head);
    parameter(processor, "gain", 0);
    processor.prepareToPlay(48000, 512);
    processor.loadFile(tone);
    wait(processor);
    juce::AudioBuffer<float> block(2, 512);

    // The recording position is speed * score time + offset.
    parameter(processor, "speed", 50);
    parameter(processor, "offset", 0.25f);
    head.samples = 96000;
    process(processor, block);
    require(std::abs(processor.recordingSeconds.load() - 1.25) < 1e-9, "Speed readout wrong");
    parameter(processor, "offset", 0);

    // 50%: the tone that starts at 0.5 s in the recording starts at 1.0 s in the score,
    // still at 440 Hz (pitch preserved, not an octave lower).
    auto out = render(processor, head, 0, 3, 512);
    const double slowOnset = onsetSeconds(out, 0);
    std::cout << "  50% onset at " << slowOnset << " s (expected 1.0), ";
    require(std::abs(slowOnset - 1.0) < 0.04, "50% speed out of sync");
    const double slowPitch = frequency(out, 0, 1.3, 2.7);
    std::cout << "pitch " << slowPitch << " Hz (expected 440)\n";
    require(std::abs(slowPitch - 440) < 4.4, "50% speed changed pitch");

    // Starting mid-recording (a seek) keeps the same alignment.
    out = render(processor, head, 0.6, 1.0, 512);
    const double seekOnset = onsetSeconds(out, 0.6);
    std::cout << "  50% after seek onset at " << seekOnset << " s, ";
    require(std::abs(seekOnset - 1.0) < 0.04, "50% speed out of sync after seek");

    // 200%: the onset arrives at 0.25 s in the score.
    parameter(processor, "speed", 200);
    out = render(processor, head, 0, 0.75, 512);
    const double fastOnset = onsetSeconds(out, 0);
    const double fastPitch = frequency(out, 0, 0.32, 0.70);
    std::cout << "200% onset at " << fastOnset << " s, pitch " << fastPitch << " Hz\n";
    require(std::abs(fastOnset - 0.25) < 0.04, "200% speed out of sync");
    require(std::abs(fastPitch - 440) < 4.4, "200% speed changed pitch");

    // Blocks larger than announced in prepareToPlay are processed in pieces.
    parameter(processor, "speed", 75);
    out = render(processor, head, 0, 2, 2048);
    const double bigOnset = onsetSeconds(out, 0);
    require(std::abs(bigOnset - 0.5 / 0.75) < 0.04, "Oversized blocks out of sync at 75%");

    // Back at 100%, the direct path is sample-exact again once the fade-in ends.
    parameter(processor, "speed", 100);
    out = render(processor, head, 0.75, 0.1, 512);
    const int index = 1000;
    const double time = 0.75 + index / 48000.0;
    near(out[index], 0.5f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 440.0 * time)),
        "100% speed not exact after leaving stretch mode", 0.0005f);
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    if ((argc == 4 || argc == 5) && juce::String(argv[1]) == "--write-state")
    {
        BackingProcessor processor;
        processor.loadFile(juce::File(argv[2]));
        wait(processor);
        require(processor.hasClip(), "Could not load recording for preset");
        parameter(processor, "gain", 0);
        if (argc == 5) parameter(processor, "speed", juce::String(argv[4]).getFloatValue());
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        require(juce::File(argv[3]).replaceWithData(state.getData(), state.getSize()), "Could not write preset");
        std::cout << "Preset written: " << argv[3] << '\n';
        return 0;
    }
    if (argc == 5 && juce::String(argv[1]) == "--render")
    {
        // Listening aid: plays a recording through the plugin at a speed (percent)
        // from score time 0 and writes what MuseScore would hear.
        BackingProcessor processor;
        Head head;
        processor.setPlayHead(&head);
        processor.loadFile(juce::File(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2])));
        wait(processor);
        require(processor.hasClip(), "Could not load recording to render");
        parameter(processor, "gain", 0);
        const float speed = juce::String(argv[4]).getFloatValue();
        parameter(processor, "speed", speed);
        processor.prepareToPlay(48000, 512);
        const auto output = juce::File::getCurrentWorkingDirectory().getChildFile(argv[3]);
        output.deleteFile();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(
            output.createOutputStream().release(), 48000, 2, 24, {}, 0));
        require(writer != nullptr, "Could not create output WAV");
        juce::AudioBuffer<float> block(2, 512);
        const auto total = static_cast<int64_t>(processor.getDuration() * 100.0 / speed * 48000);
        for (head.samples = 0; head.samples < total; head.samples += 512)
        {
            process(processor, block);
            writer->writeFromAudioSampleBuffer(block, 0, 512);
        }
        std::cout << "Rendered " << total / 48000.0 << " s at " << speed << "%: " << output.getFullPathName() << '\n';
        return 0;
    }
    const auto wav = makeWave();
    const auto tone = makeTone();
    int result = 0;
    try
    {
        run(wav);
        runFade(wav);
        runSpeed(tone);
        std::cout << "PASS: " << assertions << " native processor checks\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << assertions << " checks: " << error.what() << '\n';
        result = 1;
    }
    wav.deleteFile();
    tone.deleteFile();
    return result;
}
