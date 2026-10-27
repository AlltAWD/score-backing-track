// SPDX-License-Identifier: GPL-3.0-only
#include "PluginEditor.h"

BackingEditor::BackingEditor(BackingProcessor& owner)
    : AudioProcessorEditor(owner), processor(owner)
{
    setSize(620, 392);
    for (auto* label : { &title, &file, &status, &transport, &offsetLabel, &gainLabel, &hint })
    {
        addAndMakeVisible(*label);
        label->setColour(juce::Label::textColourId, juce::Colour(0xffe4eaf2));
    }
    title.setText("Score Backing Track", juce::dontSendNotification);
    title.setFont(juce::Font(24.0f, juce::Font::bold));
    offsetLabel.setText("Recording offset", juce::dontSendNotification);
    gainLabel.setText("Recording volume", juce::dontSendNotification);
    hint.setText("Positive offset skips the recording's intro; negative offset delays it. Double-click a\nslider to reset it. Use 100% playback speed with count-in off. Add this effect to Master.", juce::dontSendNotification);
    hint.setFont(juce::Font(13.0f));
    hint.setColour(juce::Label::textColourId, juce::Colour(0xffacb9cc));
    for (auto* button : { &choose, &remove }) addAndMakeVisible(*button);
    const float nudgeSeconds[] = { -0.1f, -0.01f, 0.01f, 0.1f };
    for (int i = 0; i < 4; ++i)
    {
        addAndMakeVisible(nudges[i]);
        nudges[i].onClick = [this, step = nudgeSeconds[i]] { nudgeOffset(step); };
    }
    addAndMakeVisible(mute);
    for (auto* slider : { &offset, &volume })
    {
        addAndMakeVisible(*slider);
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 105, 26);
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    }
    offset.setTextValueSuffix(" s");
    offset.setNumDecimalPlacesToDisplay(3);
    volume.setTextValueSuffix(" dB");
    volume.setNumDecimalPlacesToDisplay(1);
    for (auto [slider, id] : { std::pair { &offset, "offset" }, std::pair { &volume, "gain" } })
    {
        auto* parameter = processor.parameters.getParameter(id);
        slider->setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    }
    offsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "offset", offset);
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gain", volume);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "mute", mute);
    choose.onClick = [this] { chooseFile(); };
    remove.onClick = [this] { processor.clearFile(); };
    timerCallback();
    startTimerHz(10);
}

void BackingEditor::chooseFile()
{
    if (chooser != nullptr) return;
    chooser = std::make_unique<juce::FileChooser>("Choose a backing recording",
        juce::File(processor.getFilePath()), "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff", false);
    const juce::Component::SafePointer<BackingEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& picker)
        {
            if (safe == nullptr) return;
            const auto chosen = picker.getResult();
            if (chosen.existsAsFile()) safe->processor.loadFile(chosen);
            // Do not destroy the picker while its callback is on the stack.
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->chooser.reset(); });
        });
}

void BackingEditor::nudgeOffset(float seconds)
{
    auto* parameter = processor.parameters.getParameter("offset");
    const float current = processor.parameters.getRawParameterValue("offset")->load();
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(current + seconds));
    parameter->endChangeGesture();
}

void BackingEditor::paint(juce::Graphics& graphics) { graphics.fillAll(juce::Colour(0xff172235)); }

void BackingEditor::resized()
{
    auto area = getLocalBounds().reduced(22);
    title.setBounds(area.removeFromTop(38));
    file.setBounds(area.removeFromTop(28));
    auto buttons = area.removeFromTop(34);
    choose.setBounds(buttons.removeFromLeft(180).reduced(0, 2));
    buttons.removeFromLeft(12);
    remove.setBounds(buttons.removeFromLeft(85).reduced(0, 2));
    area.removeFromTop(12);
    auto row = area.removeFromTop(38);
    offsetLabel.setBounds(row.removeFromLeft(145)); offset.setBounds(row);
    row = area.removeFromTop(30);
    row.removeFromLeft(145);
    for (auto& nudge : nudges)
    {
        nudge.setBounds(row.removeFromLeft(72).reduced(0, 2));
        row.removeFromLeft(8);
    }
    area.removeFromTop(4);
    row = area.removeFromTop(38);
    gainLabel.setBounds(row.removeFromLeft(145)); volume.setBounds(row);
    mute.setBounds(area.removeFromTop(30));
    transport.setBounds(area.removeFromTop(25));
    status.setBounds(area.removeFromTop(25));
    hint.setBounds(area);
}

void BackingEditor::timerCallback()
{
    // A restored recording that failed to load still names its file; the status says why.
    auto path = processor.getFilePath();
    if (path.isEmpty()) path = processor.getRequestedPath();
    file.setText(path.isEmpty() ? "Choose a recording to get started" : juce::File(path).getFileName(), juce::dontSendNotification);
    file.setTooltip(path);
    remove.setEnabled(processor.hasClip() || processor.isLoading());
    const auto error = processor.getError();
    status.setText(error.isNotEmpty() ? error : processor.isLoading() ? "Loading recording..." :
        processor.hasClip() ? "Recording length: " + juce::String(processor.getDuration(), 2) + " seconds" : "WAV, MP3, FLAC, OGG and AIFF", juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, error.isNotEmpty() ? juce::Colour(0xffffb4a8) : juce::Colour(0xffacb9cc));
    const auto text = !processor.hostAvailable.load() ? "Press Play in MuseScore to follow its timeline" :
        juce::String(processor.hostPlaying.load() ? "Playing" : "Paused")
        + "    Score: " + juce::String(processor.hostSeconds.load(), 3)
        + " s    Recording: " + juce::String(processor.recordingSeconds.load(), 3) + " s";
    transport.setText(text, juce::dontSendNotification);
}
