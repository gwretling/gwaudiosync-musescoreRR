#include "PluginEditor.h"

TrackSyncedAudioLoaderAudioProcessorEditor::TrackSyncedAudioLoaderAudioProcessorEditor (TrackSyncedAudioLoaderAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    titleLabel.setText ("GW Track Synced Audio Loader", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions { 22.0f, juce::Font::bold }));
    addAndMakeVisible (titleLabel);

    fileLabel.setText ("Ingen fil laddad", juce::dontSendNotification);
    fileLabel.setJustificationType (juce::Justification::centred);
    fileLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (fileLabel);

    statusLabel.setText ("Ladda en fil. Uppspelning startar vid projektets början och följer hostens transport.", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centredLeft);
    gainLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (gainLabel);

    loadButton.addListener (this);
    clearButton.addListener (this);
    loopButton.addListener (this);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (loopButton);

    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    gainSlider.setRange (0.0, 2.0, 0.01);
    gainSlider.setSkewFactor (1.0);
    gainSlider.addListener (this);
    addAndMakeVisible (gainSlider);

    setSize (560, 220);
    refreshUiFromProcessor();
    startTimerHz (8);
}

TrackSyncedAudioLoaderAudioProcessorEditor::~TrackSyncedAudioLoaderAudioProcessorEditor()
{
    loadButton.removeListener (this);
    clearButton.removeListener (this);
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
    area.removeFromTop (8);
    fileLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (16);

    auto buttonRow = area.removeFromTop (30);
    loadButton.setBounds (buttonRow.removeFromLeft (170).reduced (4, 0));
    clearButton.setBounds (buttonRow.removeFromLeft (120).reduced (4, 0));
    loopButton.setBounds (buttonRow.removeFromLeft (110).reduced (8, 0));

    area.removeFromTop (18);

    auto gainRow = area.removeFromTop (30);
    gainLabel.setBounds (gainRow.removeFromLeft (60));
    gainSlider.setBounds (gainRow.reduced (4, 0));
}

void TrackSyncedAudioLoaderAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &loadButton)
    {
        launchFileChooser();
        return;
    }

    if (button == &clearButton)
    {
        audioProcessor.clearLoadedFile();
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
}

void TrackSyncedAudioLoaderAudioProcessorEditor::refreshUiFromProcessor()
{
    fileLabel.setText (audioProcessor.getLoadedFileName(), juce::dontSendNotification);
    clearButton.setEnabled (audioProcessor.hasLoadedFile());

    loopButton.setToggleState (audioProcessor.isLoopEnabled(), juce::dontSendNotification);
    gainSlider.setValue (audioProcessor.getGainLinear(), juce::dontSendNotification);

    juce::String text = audioProcessor.getTransportSummary();
    if (audioProcessor.hasLoadedFile())
        text << " • Följ play/stop/seek";
    statusLabel.setText (text, juce::dontSendNotification);
}

void TrackSyncedAudioLoaderAudioProcessorEditor::launchFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Välj ljudfil",
        juce::File{},
        audioProcessor.getSupportedWildcard());

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto chosenFile = chooser.getResult();

            if (chosenFile.existsAsFile())
            {
                const auto ok = audioProcessor.loadAudioFile (chosenFile);
                statusLabel.setText (ok ? "Fil laddad. Starta hostens transport."
                                        : "Kunde inte läsa filen.",
                                     juce::dontSendNotification);
            }

            refreshUiFromProcessor();
            fileChooser.reset();
        });
}
