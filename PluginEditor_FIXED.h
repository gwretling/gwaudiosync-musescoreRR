#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <memory>

class TrackSyncedAudioLoaderAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                                    private juce::Button::Listener,
                                                    private juce::Slider::Listener,
                                                    private juce::Timer
{
public:
    explicit TrackSyncedAudioLoaderAudioProcessorEditor (TrackSyncedAudioLoaderAudioProcessor&);
    ~TrackSyncedAudioLoaderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked (juce::Button* button) override;
    void sliderValueChanged (juce::Slider* slider) override;
    void timerCallback() override;
    void refreshUiFromProcessor();
    void launchFileChooser();

    TrackSyncedAudioLoaderAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label fileLabel;
    juce::Label statusLabel;
    juce::Label gainLabel;

    juce::TextButton loadButton { "Ladda ljudfil" };
    juce::TextButton clearButton { "Rensa" };
    juce::ToggleButton loopButton { "Loop" };
    juce::Slider gainSlider;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackSyncedAudioLoaderAudioProcessorEditor)
};
