#include "PluginEditor.h"

TrackSyncedAudioLoaderAudioProcessorEditor::TrackSyncedAudioLoaderAudioProcessorEditor (TrackSyncedAudioLoaderAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    titleLabel.setText ("Audio & Video Loader", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions { 24.0f, juce::Font::bold }));
    addAndMakeVisible (titleLabel);

    audioFileLabel.setText ("Ingen ljudfil laddad", juce::dontSendNotification);
    audioFileLabel.setJustificationType (juce::Justification::centred);
    audioFileLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (audioFileLabel);

    videoFileLabel.setText ("Ingen videofil laddad", juce::dontSendNotification);
    videoFileLabel.setJustificationType (juce::Justification::centred);
    videoFileLabel.setColour (juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible (videoFileLabel);

    statusLabel.setText ("Mixerar ljudfil ovanpå spårets ljud. Video följer hostens transport när editorn är öppen.", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    noteLabel.setText ("Tips: På befintligt spår hörs både notation och laddad ljudfil. Video är referensbild och hålls synkad via pluginfönstret.", juce::dontSendNotification);
    noteLabel.setJustificationType (juce::Justification::centredLeft);
    noteLabel.setColour (juce::Label::textColourId, juce::Colours::silver);
    addAndMakeVisible (noteLabel);

    gainLabel.setText ("Audio gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centredLeft);
    gainLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (gainLabel);

    loadAudioButton.addListener (this);
    clearAudioButton.addListener (this);
    loadVideoButton.addListener (this);
    clearVideoButton.addListener (this);
    loopButton.addListener (this);

    addAndMakeVisible (loadAudioButton);
    addAndMakeVisible (clearAudioButton);
    addAndMakeVisible (loadVideoButton);
    addAndMakeVisible (clearVideoButton);
    addAndMakeVisible (loopButton);

    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    gainSlider.setRange (0.0, 2.0, 0.01);
    gainSlider.addListener (this);
    addAndMakeVisible (gainSlider);

    videoComponent.setAudioVolume (0.0f);
    addAndMakeVisible (videoComponent);

    setSize (760, 520);
    refreshUiFromProcessor();
    startTimerHz (12);
}

TrackSyncedAudioLoaderAudioProcessorEditor::~TrackSyncedAudioLoaderAudioProcessorEditor()
{
    loadAudioButton.removeListener (this);
    clearAudioButton.removeListener (this);
    loadVideoButton.removeListener (this);
    clearVideoButton.removeListener (this);
    loopButton.removeListener (this);
    gainSlider.removeListener (this);
}

void TrackSyncedAudioLoaderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (24, 26, 30));

    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    g.setColour (juce::Colour::fromRGB (42, 46, 54));
    g.fillRoundedRectangle (bounds, 12.0f);

    g.setColour (juce::Colour::fromRGB (86, 98, 123));
    g.drawRoundedRectangle (bounds, 12.0f, 1.5f);
}

void TrackSyncedAudioLoaderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);

    titleLabel.setBounds (area.removeFromTop (34));
    area.removeFromTop (6);
    audioFileLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    videoFileLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    noteLabel.setBounds (area.removeFromTop (34));
    area.removeFromTop (12);

    auto topButtons = area.removeFromTop (30);
    loadAudioButton.setBounds (topButtons.removeFromLeft (170).reduced (4, 0));
    clearAudioButton.setBounds (topButtons.removeFromLeft (120).reduced (4, 0));
    loadVideoButton.setBounds (topButtons.removeFromLeft (170).reduced (4, 0));
    clearVideoButton.setBounds (topButtons.removeFromLeft (120).reduced (4, 0));

    area.removeFromTop (12);

    auto gainRow = area.removeFromTop (30);
    gainLabel.setBounds (gainRow.removeFromLeft (90));
    gainSlider.setBounds (gainRow.removeFromLeft (420).reduced (4, 0));
    loopButton.setBounds (gainRow.removeFromLeft (120).reduced (8, 0));

    area.removeFromTop (12);
    videoComponent.setBounds (area.reduced (0, 0));
}

void TrackSyncedAudioLoaderAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &loadAudioButton)
    {
        launchAudioChooser();
        return;
    }

    if (button == &clearAudioButton)
    {
        audioProcessor.clearLoadedFile();
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
        audioProcessor.clearReferenceVideo();
        editorLoadedVideoPath.clear();
        videoComponent.stop();
        videoComponent.closeVideo();
        refreshUiFromProcessor();
        return;
    }

    if (button == &loopButton)
    {
        audioProcessor.setLoopEnabled (loopButton.getToggleState());
        refreshUiFromProcessor();
    }
}

void TrackSyncedAudioLoaderAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &gainSlider)
        audioProcessor.setGainLinear (static_cast<float> (gainSlider.getValue()));
}

void TrackSyncedAudioLoaderAudioProcessorEditor::timerCallback()
{
    refreshUiFromProcessor();
    syncVideoToTransport();
}

void TrackSyncedAudioLoaderAudioProcessorEditor::ensureLoadedVideoMatchesProcessor()
{
    const auto processorVideoPath = audioProcessor.getLoadedVideoPath();

    if (processorVideoPath.isEmpty())
    {
        if (editorLoadedVideoPath.isNotEmpty())
        {
            videoComponent.stop();
            videoComponent.closeVideo();
            editorLoadedVideoPath.clear();
        }
        return;
    }

    if (processorVideoPath == editorLoadedVideoPath)
        return;

    const auto result = videoComponent.load (juce::File (processorVideoPath));
    if (result.wasOk())
    {
        editorLoadedVideoPath = processorVideoPath;
        videoComponent.setAudioVolume (0.0f);
        videoComponent.setPlayPosition (audioProcessor.getCurrentHostTimeSeconds());
    }
    else
    {
        editorLoadedVideoPath.clear();
        statusLabel.setText ("Videon kunde inte öppnas i JUCE/Windows.", juce::dontSendNotification);
    }
}

void TrackSyncedAudioLoaderAudioProcessorEditor::syncVideoToTransport()
{
    ensureLoadedVideoMatchesProcessor();

    if (editorLoadedVideoPath.isEmpty() || ! videoComponent.isVideoOpen())
        return;

    const auto hostSeconds = audioProcessor.getCurrentHostTimeSeconds();

    if (audioProcessor.getHostIsPlaying())
    {
        if (std::abs (videoComponent.getPlayPosition() - hostSeconds) > 0.10)
            videoComponent.setPlayPosition (hostSeconds);

        if (! videoComponent.isPlaying())
            videoComponent.play();
    }
    else
    {
        if (videoComponent.isPlaying())
            videoComponent.stop();

        if (std::abs (videoComponent.getPlayPosition() - hostSeconds) > 0.10)
            videoComponent.setPlayPosition (hostSeconds);
    }
}

void TrackSyncedAudioLoaderAudioProcessorEditor::refreshUiFromProcessor()
{
    audioFileLabel.setText (audioProcessor.getLoadedFileName(), juce::dontSendNotification);
    videoFileLabel.setText (audioProcessor.getLoadedVideoName(), juce::dontSendNotification);

    clearAudioButton.setEnabled (audioProcessor.hasLoadedFile());
    clearVideoButton.setEnabled (audioProcessor.hasLoadedVideo());

    loopButton.setToggleState (audioProcessor.isLoopEnabled(), juce::dontSendNotification);
    gainSlider.setValue (audioProcessor.getGainLinear(), juce::dontSendNotification);

    juce::String text = audioProcessor.getTransportSummary();
    if (audioProcessor.hasLoadedFile())
        text << " • mixar ovanpå notspåret";
    statusLabel.setText (text, juce::dontSendNotification);
}

void TrackSyncedAudioLoaderAudioProcessorEditor::launchAudioChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Välj ljudfil",
        juce::File{},
        audioProcessor.getSupportedAudioWildcard());

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto chosenFile = chooser.getResult();

            if (chosenFile.existsAsFile())
            {
                const auto ok = audioProcessor.loadAudioFile (chosenFile);
                statusLabel.setText (ok ? "Ljudfil laddad. Spela från början eller valfri plats i scoret."
                                        : "Kunde inte läsa ljudfilen.",
                                     juce::dontSendNotification);
            }

            refreshUiFromProcessor();
            fileChooser.reset();
        });
}

void TrackSyncedAudioLoaderAudioProcessorEditor::launchVideoChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Välj videofil",
        juce::File{},
        audioProcessor.getSupportedVideoWildcard());

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto chosenFile = chooser.getResult();

            if (chosenFile.existsAsFile())
            {
                const auto ok = audioProcessor.setReferenceVideoFile (chosenFile);
                statusLabel.setText (ok ? "Videofil laddad. Editorn håller bilden synkad med hostens transport."
                                        : "Kunde inte registrera videofil.",
                                     juce::dontSendNotification);
            }

            refreshUiFromProcessor();
            fileChooser.reset();
        });
}
