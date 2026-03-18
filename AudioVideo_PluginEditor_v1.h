#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_video/juce_video.h>
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
    void launchAudioChooser();
    void launchVideoChooser();
    void syncVideoToTransport();
    void ensureLoadedVideoMatchesProcessor();

    TrackSyncedAudioLoaderAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label audioFileLabel;
    juce::Label videoFileLabel;
    juce::Label statusLabel;
    juce::Label gainLabel;
    juce::Label noteLabel;

    juce::TextButton loadAudioButton { "Ladda ljudfil" };
    juce::TextButton clearAudioButton { "Rensa ljud" };
    juce::TextButton loadVideoButton { "Ladda videofil" };
    juce::TextButton clearVideoButton { "Rensa video" };
    juce::ToggleButton loopButton { "Loop audio" };
    juce::Slider gainSlider;
    juce::VideoComponent videoComponent { false };

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String editorLoadedVideoPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackSyncedAudioLoaderAudioProcessorEditor)
};
