// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <atomic>

class BackingProcessor final : public juce::AudioProcessor
{
public:
    BackingProcessor();
    ~BackingProcessor() override;

    const juce::String getName() const override { return "Score Backing Track"; }
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void loadFile(const juce::File&, bool notifyHost = true);
    void clearFile();
    bool isLoading() const { return loading.load(); }
    juce::String getError() const;
    juce::String getFilePath() const;
    juce::String getRequestedPath() const;
    double getDuration() const;
    bool hasClip() const;
    // Length of the fade-in after any jump (start, seek, offset or speed change).
    void setSeekFadeSeconds(double seconds) { seekFadeSeconds = seconds; }

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<bool> hostAvailable { false }, hostPlaying { false };
    std::atomic<double> hostSeconds { 0 }, recordingSeconds { 0 };

private:
    struct Clip
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 0;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void decode(const juce::File&, uint64_t generation, bool notifyHost);
    void setError(juce::String, uint64_t generation);
    void readRecording(const Clip&, double startSeconds, int count, juce::AudioBuffer<float>& destination) const;
    void renderStretched(const Clip&, juce::AudioBuffer<float>&, double recordingStart, double speed, bool jumped);

    juce::AudioFormatManager formats;
    juce::ThreadPool loader { 1 };
    // clipLock is shared only with the audio thread and held just for swaps.
    // Everything the editor or host polls lives under infoLock instead, so a
    // poll can never make processBlock's try-lock fail and drop a block.
    mutable juce::SpinLock clipLock;
    std::unique_ptr<Clip> clip;
    mutable juce::CriticalSection infoLock;
    juce::String error;
    juce::String requestedPath, loadedPath;
    double loadedDuration = 0;
    std::atomic<bool> loading { false };
    std::atomic<uint64_t> loadGeneration { 0 };
    std::atomic<float>* offsetParameter;
    std::atomic<float>* gainParameter;
    std::atomic<float>* muteParameter;
    std::atomic<float>* speedParameter;
    double engineSampleRate = 44100;
    juce::SmoothedValue<float> gain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> entry;
    double seekFadeSeconds = 0.005;
    double lastReportedSeconds = -1, nextBlockSeconds = 0;
    bool previouslyPlaying = false;

    // Continuity tracking: a block that doesn't start where the previous one
    // ended (in recording time) is a jump and gets a short fade-in.
    const Clip* previousClip = nullptr;
    double expectedRecordingSeconds = 0, previousSpeed = 1;

    // Practice speed: pitch-preserving stretch, audio thread only after prepareToPlay.
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> stretchInput, stretchOutput;
    int stretchChunk = 0;
    bool stretchPrimed = false;
    double stretchInputSeconds = 0, stretchRemainder = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingProcessor)
};
