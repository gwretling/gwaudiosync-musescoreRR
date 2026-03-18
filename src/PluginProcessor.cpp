#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

TrackSyncedAudioLoaderAudioProcessor::TrackSyncedAudioLoaderAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    formatManager.registerBasicFormats();
}

TrackSyncedAudioLoaderAudioProcessor::~TrackSyncedAudioLoaderAudioProcessor() = default;

const juce::String TrackSyncedAudioLoaderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TrackSyncedAudioLoaderAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TrackSyncedAudioLoaderAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TrackSyncedAudioLoaderAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TrackSyncedAudioLoaderAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TrackSyncedAudioLoaderAudioProcessor::getNumPrograms()
{
    return 1;
}

int TrackSyncedAudioLoaderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TrackSyncedAudioLoaderAudioProcessor::setCurrentProgram (int)
{
}

const juce::String TrackSyncedAudioLoaderAudioProcessor::getProgramName (int)
{
    return {};
}

void TrackSyncedAudioLoaderAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void TrackSyncedAudioLoaderAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentHostSampleRate.store (sampleRate);
}

void TrackSyncedAudioLoaderAudioProcessor::releaseResources()
{
}

#if ! JucePlugin_IsMidiEffect
bool TrackSyncedAudioLoaderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainInput  = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput.isDisabled() || mainOutput.isDisabled())
        return false;

    if (mainInput != mainOutput)
        return false;

    return mainOutput == juce::AudioChannelSet::mono()
        || mainOutput == juce::AudioChannelSet::stereo();
}
#endif

std::shared_ptr<TrackSyncedAudioLoaderAudioProcessor::LoadedFileData> TrackSyncedAudioLoaderAudioProcessor::getLoadedFileSnapshot() const
{
    std::scoped_lock lock (fileMutex);
    return loadedFile;
}

float TrackSyncedAudioLoaderAudioProcessor::readSampleLinear (const LoadedFileData& file, int channel, double sourceIndex) const
{
    const auto numSamples = file.buffer.getNumSamples();

    if (numSamples <= 0 || channel < 0 || channel >= file.buffer.getNumChannels())
        return 0.0f;

    if (sourceIndex < 0.0)
        return 0.0f;

    if (sourceIndex >= static_cast<double> (numSamples - 1))
    {
        if (sourceIndex < static_cast<double> (numSamples))
            return file.buffer.getSample (channel, numSamples - 1);

        return 0.0f;
    }

    const auto i0 = static_cast<int> (sourceIndex);
    const auto i1 = juce::jmin (i0 + 1, numSamples - 1);
    const auto frac = static_cast<float> (sourceIndex - static_cast<double> (i0));

    const auto* data = file.buffer.getReadPointer (channel);
    const auto s0 = data[i0];
    const auto s1 = data[i1];
    return s0 + frac * (s1 - s0);
}

double TrackSyncedAudioLoaderAudioProcessor::wrapSourceIndex (double sourceIndex, int numSamples) const
{
    if (numSamples <= 0)
        return 0.0;

    const auto length = static_cast<double> (numSamples);
    auto wrapped = std::fmod (sourceIndex, length);

    if (wrapped < 0.0)
        wrapped += length;

    return wrapped;
}

void TrackSyncedAudioLoaderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto file = getLoadedFileSnapshot();
    if (file == nullptr || file->buffer.getNumChannels() <= 0 || file->buffer.getNumSamples() <= 0)
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = file == nullptr ? "Ingen fil" : "Fil tom";
        return;
    }

    auto* playHead = getPlayHead();
    if (playHead == nullptr)
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = "Ingen host-transport";
        return;
    }

    const auto position = playHead->getPosition();
    if (! position.has_value())
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = "Ingen positionsdata";
        return;
    }

    if (! position->getIsPlaying())
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = "Stoppad";
        return;
    }

    const auto timeInSamples = position->getTimeInSamples();
    if (! timeInSamples.has_value())
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = "Ingen sample-position";
        return;
    }

    const auto hostStartSample = static_cast<double> (*timeInSamples);
    const auto hostRate = juce::jmax (1.0, currentHostSampleRate.load());
    const auto resampleRatio = file->sampleRate / hostRate;
    const auto numFileChannels = file->buffer.getNumChannels();
    const auto shouldLoop = loopEnabled.load();
    const auto gain = gainLinear.load();

    {
        std::scoped_lock lock (transportMutex);
        transportSummary = shouldLoop ? "Spelar (loop)" : "Spelar";
    }

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const int sourceChannel = juce::jmin (channel, numFileChannels - 1);
        auto* out = buffer.getWritePointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto sourceIndex = (hostStartSample + static_cast<double> (sample)) * resampleRatio;

            if (shouldLoop)
                sourceIndex = wrapSourceIndex (sourceIndex, file->buffer.getNumSamples());

            out[sample] = gain * readSampleLinear (*file, sourceChannel, sourceIndex);
        }
    }
}

bool TrackSyncedAudioLoaderAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* TrackSyncedAudioLoaderAudioProcessor::createEditor()
{
    return new TrackSyncedAudioLoaderAudioProcessorEditor (*this);
}

void TrackSyncedAudioLoaderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("TrackSyncedAudioLoaderState");
    state.setProperty ("filePath", getLoadedFilePath(), nullptr);
    state.setProperty ("loopEnabled", isLoopEnabled(), nullptr);
    state.setProperty ("gainLinear", getGainLinear(), nullptr);

    if (auto xml = state.createXml())
        juce::copyXmlToBinary (*xml, destData);
}

void TrackSyncedAudioLoaderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xmlState = juce::getXmlFromBinary (data, sizeInBytes);
    if (xmlState == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xmlState);
    if (! state.isValid())
        return;

    setLoopEnabled (static_cast<bool> (state.getProperty ("loopEnabled", false)));
    setGainLinear (static_cast<float> (state.getProperty ("gainLinear", 1.0f)));

    const auto storedPath = state.getProperty ("filePath").toString();
    if (storedPath.isNotEmpty())
    {
        const juce::File file (storedPath);
        if (file.existsAsFile())
            loadAudioFile (file);
        else
            clearLoadedFile();
    }
    else
    {
        clearLoadedFile();
    }
}

bool TrackSyncedAudioLoaderAudioProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr || reader->numChannels <= 0 || reader->lengthInSamples <= 0)
        return false;

    auto newFile = std::make_shared<LoadedFileData>();
    newFile->buffer.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));

    if (! reader->read (&newFile->buffer,
                        0,
                        static_cast<int> (reader->lengthInSamples),
                        0,
                        true,
                        true))
    {
        return false;
    }

    newFile->sampleRate = reader->sampleRate;
    newFile->filePath = file.getFullPathName();
    newFile->fileName = file.getFileName();

    {
        std::scoped_lock lock (fileMutex);
        loadedFile = std::move (newFile);
    }

    return true;
}

void TrackSyncedAudioLoaderAudioProcessor::clearLoadedFile()
{
    std::scoped_lock lock (fileMutex);
    loadedFile.reset();
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getLoadedFileName() const
{
    if (const auto file = getLoadedFileSnapshot())
        return file->fileName;

    return "Ingen fil laddad";
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getLoadedFilePath() const
{
    if (const auto file = getLoadedFileSnapshot())
        return file->filePath;

    return {};
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getSupportedWildcard() const
{
    return formatManager.getWildcardForAllFormats();
}

bool TrackSyncedAudioLoaderAudioProcessor::hasLoadedFile() const
{
    return getLoadedFileSnapshot() != nullptr;
}

void TrackSyncedAudioLoaderAudioProcessor::setLoopEnabled (bool shouldLoop)
{
    loopEnabled.store (shouldLoop);
}

bool TrackSyncedAudioLoaderAudioProcessor::isLoopEnabled() const
{
    return loopEnabled.load();
}

void TrackSyncedAudioLoaderAudioProcessor::setGainLinear (float newGain)
{
    gainLinear.store (juce::jlimit (0.0f, 2.0f, newGain));
}

float TrackSyncedAudioLoaderAudioProcessor::getGainLinear() const
{
    return gainLinear.load();
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getTransportSummary() const
{
    std::scoped_lock lock (transportMutex);
    return transportSummary;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrackSyncedAudioLoaderAudioProcessor();
}
