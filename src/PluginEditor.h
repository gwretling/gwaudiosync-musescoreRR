#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_video/juce_video.h>
#include "PluginProcessor.h"
#include <memory>

class AudioVideoLoaderEditor : public juce::AudioProcessorEditor,
                               private juce::Button::Listener,
                               private juce::Slider::Listener,
                               private juce::Timer
{
public:
    explicit AudioVideoLoaderEditor (AudioVideoLoaderProcessor&);
    ~AudioVideoLoaderEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class VideoWindow : public juce::DocumentWindow,
                        private juce::Timer
    {
    public:
        explicit VideoWindow (AudioVideoLoaderProcessor&);
        ~VideoWindow() override;

        bool openVideo (const juce::File& file);
        void clearVideo();
        void closeButtonPressed() override;

    private:
        void timerCallback() override;

        AudioVideoLoaderProcessor& processor;
        juce::VideoComponent* videoComponent = nullptr;
        juce::String currentVideoPath;
    };

    void buttonClicked (juce::Button* button) override;
    void sliderValueChanged (juce::Slider* slider) override;
    void timerCallback() override;

    void refreshUiFromProcessor();
    void launchAudioChooser();
    void launchVideoChooser();
    void ensureVideoWindowMatchesProcessor();

    AudioVideoLoaderProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label audioFileLabel;
    juce::Label videoFileLabel;
    juce::Label statusLabel;
    juce::Label gainLabel;
    juce::Label noteLabel;

    juce::TextButton loadAudioButton { "Ladda Audio" };
    juce::TextButton clearAudioButton { "Rensa Audio" };
    juce::TextButton loadVideoButton { "Ladda Video" };
    juce::TextButton clearVideoButton { "Rensa Video" };
    juce::ToggleButton loopButton { "Loop audio" };
    juce::Slider gainSlider;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<VideoWindow> videoWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioVideoLoaderEditor)
};
