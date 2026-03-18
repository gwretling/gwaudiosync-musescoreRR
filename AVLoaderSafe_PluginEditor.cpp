#include "PluginEditor.h"
#include <cmath>

AudioVideoLoaderEditor::VideoWindow::VideoWindow (AudioVideoLoaderProcessor& p)
    : DocumentWindow ("Video Reference",
                      juce::Colours::black,
                      juce::DocumentWindow::closeButton),
      processor (p)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    videoComponent = new juce::VideoComponent (false);
    videoComponent->setAudioVolume (0.0f);
    setContentOwned (videoComponent, false);
    centreWithSize (960, 540);
    setVisible (false);
}

AudioVideoLoaderEditor::VideoWindow::~VideoWindow()
{
    stopTimer();
}

bool AudioVideoLoaderEditor::VideoWindow::openVideo (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    if (videoComponent == nullptr)
        return false;

    videoComponent->stop();
    videoComponent->closeVideo();

    const auto result = videoComponent->load (file);
    if (result.failed())
        return false;

    currentVideoPath = file.getFullPathName();
    videoComponent->setAudioVolume (0.0f);
    videoComponent->setPlayPosition (processor.getCurrentHostTimeSeconds());
    setName ("Video Reference - " + file.getFileName());
    setVisible (true);
    toFront (true);
    startTimerHz (12);
    return true;
}

void AudioVideoLoaderEditor::VideoWindow::clearVideo()
{
    stopTimer();
    currentVideoPath.clear();
    if (videoComponent != nullptr)
    {
        videoComponent->stop();
        videoComponent->closeVideo();
    }
    setVisible (false);
}

void AudioVideoLoaderEditor::VideoWindow::closeButtonPressed()
{
    setVisible (false);
}

void AudioVideoLoaderEditor::VideoWindow::timerCallback()
{
    if (videoComponent == nullptr || currentVideoPath.isEmpty() || ! videoComponent->isVideoOpen())
        return;

    const auto hostSeconds = processor.getCurrentHostTimeSeconds();

    if (processor.getHostIsPlaying())
    {
        if (std::abs (videoComponent->getPlayPosition() - hostSeconds) > 0.10)
            videoComponent->setPlayPosition (hostSeconds);

        if (! videoComponent->isPlaying())
            videoComponent->play();
    }
    else
    {
        if (videoComponent->isPlaying())
            videoComponent->stop();

        if (std::abs (videoComponent->getPlayPosition() - hostSeconds) > 0.10)
            videoComponent->setPlayPosition (hostSeconds);
    }
}

AudioVideoLoaderEditor::AudioVideoLoaderEditor (AudioVideoLoaderProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    titleLabel.setText ("Audio & Video Loader", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont (juce::Font (juce::FontOptions { 24.0f, juce::Font::bold }));
    addAndMakeVisible (titleLabel);

    audioFileLabel.setJustificationType (juce::Justification::centredLeft);
    audioFileLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (audioFileLabel);

    videoFileLabel.setJustificationType (juce::Justification::centredLeft);
    videoFileLabel.setColour (juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible (videoFileLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    noteLabel.setText ("Audio och video startar först efter att du själv har laddat en fil. Video öppnas i separat referensfönster för stabilare drift i MuseScore.", juce::dontSendNotification);
    noteLabel.setJustificationType (juce::Justification::topLeft);
    noteLabel.setColour (juce::Label::textColourId, juce::Colours::silver);
    addAndMakeVisible (noteLabel);

    gainLabel.setText ("Audio gain", juce::dontSendNotification);
    gainLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (gainLabel);

    for (auto* b : std::initializer_list<juce::Button*> { &loadAudioButton, &clearAudioButton, &loadVideoButton, &clearVideoButton, &loopButton })
    {
        b->addListener (this);
        addAndMakeVisible (b);
    }

    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    gainSlider.setRange (0.0, 2.0, 0.01);
    gainSlider.addListener (this);
    addAndMakeVisible (gainSlider);

    setSize (760, 260);
    refreshUiFromProcessor();
    startTimerHz (12);
}

AudioVideoLoaderEditor::~AudioVideoLoaderEditor()
{
    stopTimer();

    for (auto* b : std::initializer_list<juce::Button*> { &loadAudioButton, &clearAudioButton, &loadVideoButton, &clearVideoButton, &loopButton })
        b->removeListener (this);

    gainSlider.removeListener (this);
}

void AudioVideoLoaderEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (24, 26, 30));

    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    g.setColour (juce::Colour::fromRGB (42, 46, 54));
    g.fillRoundedRectangle (bounds, 12.0f);

    g.setColour (juce::Colour::fromRGB (86, 98, 123));
    g.drawRoundedRectangle (bounds, 12.0f, 1.5f);
}

void AudioVideoLoaderEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (34));
    area.removeFromTop (8);

    auto row1 = area.removeFromTop (28);
    loadAudioButton.setBounds (row1.removeFromLeft (140).reduced (4, 0));
    clearAudioButton.setBounds (row1.removeFromLeft (130).reduced (4, 0));
    audioFileLabel.setBounds (row1.reduced (8, 0));

    area.removeFromTop (8);

    auto row2 = area.removeFromTop (28);
    loadVideoButton.setBounds (row2.removeFromLeft (140).reduced (4, 0));
    clearVideoButton.setBounds (row2.removeFromLeft (130).reduced (4, 0));
    videoFileLabel.setBounds (row2.reduced (8, 0));

    area.removeFromTop (12);

    auto row3 = area.removeFromTop (28);
    gainLabel.setBounds (row3.removeFromLeft (90));
    gainSlider.setBounds (row3.removeFromLeft (420).reduced (4, 0));
    loopButton.setBounds (row3.removeFromLeft (120).reduced (6, 0));

    area.removeFromTop (10);
    statusLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);
    noteLabel.setBounds (area.removeFromTop (56));
}

void AudioVideoLoaderEditor::buttonClicked (juce::Button* button)
{
    if (button == &loadAudioButton)
    {
        launchAudioChooser();
        return;
    }

    if (button == &clearAudioButton)
    {
        audioProcessor.clearLoadedAudio();
        refreshUiFromProcessor();
        return;
    }

    if (button == &loadVideoButton)
    {
        launchVideoChooser();
        return;
    }

    if (button == &clearVideoButton)
    {
        audioProcessor.clearLoadedVideo();
        if (videoWindow != nullptr)
            videoWindow->clearVideo();
        refreshUiFromProcessor();
        return;
    }

    if (button == &loopButton)
    {
        audioProcessor.setLoopEnabled (loopButton.getToggleState());
        refreshUiFromProcessor();
    }
}

void AudioVideoLoaderEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &gainSlider)
        audioProcessor.setGainLinear (static_cast<float> (gainSlider.getValue()));
}

void AudioVideoLoaderEditor::timerCallback()
{
    refreshUiFromProcessor();
    ensureVideoWindowMatchesProcessor();
}

void AudioVideoLoaderEditor::refreshUiFromProcessor()
{
    audioFileLabel.setText (audioProcessor.getLoadedAudioName(), juce::dontSendNotification);
    videoFileLabel.setText (audioProcessor.getLoadedVideoName(), juce::dontSendNotification);
    statusLabel.setText (audioProcessor.getTransportSummary(), juce::dontSendNotification);

    clearAudioButton.setEnabled (audioProcessor.hasLoadedAudio());
    clearVideoButton.setEnabled (audioProcessor.hasLoadedVideo());
    loopButton.setToggleState (audioProcessor.isLoopEnabled(), juce::dontSendNotification);
    gainSlider.setValue (audioProcessor.getGainLinear(), juce::dontSendNotification);
}

void AudioVideoLoaderEditor::ensureVideoWindowMatchesProcessor()
{
    const auto videoPath = audioProcessor.getLoadedVideoPath();
    if (videoPath.isEmpty())
        return;

    if (videoWindow == nullptr)
        return;

    if (! videoWindow->isVisible())
        return;
}

void AudioVideoLoaderEditor::launchAudioChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Välj ljudfil",
        juce::File{},
        audioProcessor.getSupportedAudioWildcard());

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto chosen = chooser.getResult();
            if (chosen.existsAsFile())
            {
                const auto ok = audioProcessor.loadAudioFile (chosen);
                statusLabel.setText (ok ? "Ljudfil laddad och redo för host-synkad uppspelning."
                                        : "Kunde inte läsa ljudfilen.",
                                     juce::dontSendNotification);
            }
            refreshUiFromProcessor();
            fileChooser.reset();
        });
}

void AudioVideoLoaderEditor::launchVideoChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Välj videofil",
        juce::File{},
        audioProcessor.getSupportedVideoWildcard());

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto chosen = chooser.getResult();
            if (chosen.existsAsFile())
            {
                if (audioProcessor.setVideoFile (chosen))
                {
                    if (videoWindow == nullptr)
                        videoWindow = std::make_unique<VideoWindow> (audioProcessor);

                    const auto opened = videoWindow->openVideo (chosen);
                    statusLabel.setText (opened ? "Videofil laddad. Referensfönstret följer hostens transport."
                                                : "Videon kunde inte öppnas i Windows/JUCE.",
                                         juce::dontSendNotification);
                }
                else
                {
                    statusLabel.setText ("Kunde inte registrera videofil.", juce::dontSendNotification);
                }
            }
            refreshUiFromProcessor();
            fileChooser.reset();
        });
}
