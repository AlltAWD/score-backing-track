// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

class BackingProcessor final : public juce::AudioProcessor
{
public:
    BackingProcessor();
    ~BackingProcessor() override;

    const juce::String getName() const override { return "Muse Backing Track"; }
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
    double getDuration() const;
    bool hasClip() const;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<bool> hostAvailable { false }, hostPlaying { false };
    std::atomic<double> hostSeconds { 0 }, recordingSeconds { 0 };
    std::atomic<uint64_t> processedBlocks { 0 };

private:
    struct Clip
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 0;
        juce::String path;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void decode(const juce::File&, uint64_t generation, bool notifyHost);
    void setError(juce::String, uint64_t generation);

    juce::AudioFormatManager formats;
    juce::ThreadPool loader { 1 };
    mutable juce::SpinLock clipLock;
    std::unique_ptr<Clip> clip;
    mutable juce::CriticalSection errorLock;
    juce::String error;
    juce::String requestedPath;
    std::atomic<bool> loading { false };
    std::atomic<uint64_t> loadGeneration { 0 };
    std::atomic<float>* offsetParameter;
    std::atomic<float>* gainParameter;
    std::atomic<float>* muteParameter;
    double engineSampleRate = 44100;
    juce::SmoothedValue<float> gain;
    double lastReportedSeconds = -1, nextBlockSeconds = 0;
    bool previouslyPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingProcessor)
};
