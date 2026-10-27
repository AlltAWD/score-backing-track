// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class BackingEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BackingEditor(BackingProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseFile();
    void nudgeOffset(float seconds);
    BackingProcessor& processor;
    juce::Label title, file, status, transport, offsetLabel, gainLabel, hint;
    juce::TextButton choose { "Choose recording..." }, remove { "Remove" };
    juce::TextButton nudges[4] { juce::TextButton { "-0.1 s" }, juce::TextButton { "-0.01 s" },
                                 juce::TextButton { "+0.01 s" }, juce::TextButton { "+0.1 s" } };
    juce::ToggleButton mute { "Mute recording" };
    juce::Slider offset, volume;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> offsetAttachment, volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingEditor)
};
