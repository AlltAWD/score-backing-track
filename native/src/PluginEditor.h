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
    void setParameter(const char* id, float value);
    BackingProcessor& processor;
    juce::Label title, file, status, transport, offsetLabel, gainLabel, speedLabel, hint;
    juce::TextButton choose { "Choose recording..." }, remove { "Remove" };
    juce::TextButton nudges[4] { juce::TextButton { "-0.1 s" }, juce::TextButton { "-0.01 s" },
                                 juce::TextButton { "+0.01 s" }, juce::TextButton { "+0.1 s" } };
    juce::TextButton speedPresets[3] { juce::TextButton { "50%" }, juce::TextButton { "75%" },
                                       juce::TextButton { "100%" } };
    juce::ToggleButton mute { "Mute recording" };
    juce::Slider offset, volume, speed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> offsetAttachment, volumeAttachment, speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackingEditor)
};
