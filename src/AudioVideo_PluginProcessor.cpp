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

    auto* hostPlayHead = getPlayHead();
    if (hostPlayHead == nullptr)
    {
        hostIsPlaying.store (false);
        std::scoped_lock lock (transportMutex);
        transportSummary = "Ingen host-transport";
        return;
    }

    const auto position = hostPlayHead->getPosition();
    if (! position.hasValue())
    {
        hostIsPlaying.store (false);
        std::scoped_lock lock (transportMutex);
        transportSummary = "Ingen positionsdata";
        return;
    }

    const auto isPlayingNow = position->getIsPlaying();
    hostIsPlaying.store (isPlayingNow);

    if (! isPlayingNow)
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = hasLoadedFile() ? "Stoppad • ljudfil redo" : "Stoppad";
        return;
    }

    const auto timeInSamples = position->getTimeInSamples();
    if (! timeInSamples.hasValue())
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = "Ingen sample-position";
        return;
    }

    const auto hostStartSample = static_cast<double> (*timeInSamples);
    const auto hostRate = juce::jmax (1.0, currentHostSampleRate.load());

    if (const auto timeInSeconds = position->getTimeInSeconds(); timeInSeconds.hasValue())
        currentHostTimeSeconds.store (*timeInSeconds);
    else
        currentHostTimeSeconds.store (hostStartSample / hostRate);

    const auto file = getLoadedFileSnapshot();
    if (file == nullptr || file->buffer.getNumChannels() <= 0 || file->buffer.getNumSamples() <= 0)
    {
        std::scoped_lock lock (transportMutex);
        transportSummary = hasLoadedVideo() ? "Spelar • video aktiv, ingen ljudfil" : "Spelar • ingen ljudfil";
        return;
    }

    const auto resampleRatio = file->sampleRate / hostRate;
    const auto numFileChannels = file->buffer.getNumChannels();
    const auto shouldLoop = loopEnabled.load();
    const auto gain = gainLinear.load();

    {
        std::scoped_lock lock (transportMutex);
        transportSummary = shouldLoop ? "Spelar • audio loop" : "Spelar • audio synkad";
        if (hasLoadedVideo())
            transportSummary << " • video referens";
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

            if (! shouldLoop && sourceIndex >= static_cast<double> (file->buffer.getNumSamples()))
                continue;

            out[sample] += gain * readSampleLinear (*file, sourceChannel, sourceIndex);
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
    juce::ValueTree state ("AudioVideoLoaderState");
    state.setProperty ("audioPath", getLoadedFilePath(), nullptr);
    state.setProperty ("videoPath", getLoadedVideoPath(), nullptr);
    state.setProperty ("loopEnabled", isLoopEnabled(), nullptr);
    state.setProperty ("gainLinear", getGainLinear(), nullptr);

    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, destData);
}

void TrackSyncedAudioLoaderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xmlState = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
    if (xmlState == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xmlState);
    if (! state.isValid())
        return;

    setLoopEnabled (static_cast<bool> (state.getProperty ("loopEnabled", false)));
    setGainLinear (static_cast<float> (state.getProperty ("gainLinear", 1.0f)));

    const auto audioPath = state.getProperty ("audioPath").toString();
    if (audioPath.isNotEmpty())
    {
        const juce::File file (audioPath);
        if (file.existsAsFile())
            loadAudioFile (file);
        else
            clearLoadedFile();
    }
    else
    {
        clearLoadedFile();
    }

    const auto videoPath = state.getProperty ("videoPath").toString();
    if (videoPath.isNotEmpty())
    {
        const juce::File file (videoPath);
        if (file.existsAsFile())
            setReferenceVideoFile (file);
        else
            clearReferenceVideo();
    }
    else
    {
        clearReferenceVideo();
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

bool TrackSyncedAudioLoaderAudioProcessor::setReferenceVideoFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    std::scoped_lock lock (videoMutex);
    loadedVideoPath = file.getFullPathName();
    loadedVideoName = file.getFileName();
    return true;
}

void TrackSyncedAudioLoaderAudioProcessor::clearReferenceVideo()
{
    std::scoped_lock lock (videoMutex);
    loadedVideoPath.clear();
    loadedVideoName.clear();
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getLoadedFileName() const
{
    if (const auto file = getLoadedFileSnapshot())
        return file->fileName;

    return "Ingen ljudfil laddad";
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getLoadedFilePath() const
{
    if (const auto file = getLoadedFileSnapshot())
        return file->filePath;

    return {};
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getLoadedVideoName() const
{
    std::scoped_lock lock (videoMutex);
    return loadedVideoName.isNotEmpty() ? loadedVideoName : "Ingen videofil laddad";
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getLoadedVideoPath() const
{
    std::scoped_lock lock (videoMutex);
    return loadedVideoPath;
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getSupportedAudioWildcard() const
{
    return formatManager.getWildcardForAllFormats();
}

juce::String TrackSyncedAudioLoaderAudioProcessor::getSupportedVideoWildcard() const
{
    return "*.mp4;*.mov;*.m4v;*.avi;*.wmv;*.mkv";
}

bool TrackSyncedAudioLoaderAudioProcessor::hasLoadedFile() const
{
    return getLoadedFileSnapshot() != nullptr;
}

bool TrackSyncedAudioLoaderAudioProcessor::hasLoadedVideo() const
{
    std::scoped_lock lock (videoMutex);
    return loadedVideoPath.isNotEmpty();
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

double TrackSyncedAudioLoaderAudioProcessor::getCurrentHostTimeSeconds() const
{
    return currentHostTimeSeconds.load();
}

bool TrackSyncedAudioLoaderAudioProcessor::getHostIsPlaying() const
{
    return hostIsPlaying.load();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrackSyncedAudioLoaderAudioProcessor();
}
