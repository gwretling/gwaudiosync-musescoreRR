#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

AudioVideoLoaderProcessor::AudioVideoLoaderProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    formatManager.registerBasicFormats();
}

AudioVideoLoaderProcessor::~AudioVideoLoaderProcessor() = default;

const juce::String AudioVideoLoaderProcessor::getName() const                     { return JucePlugin_Name; }
bool AudioVideoLoaderProcessor::acceptsMidi() const                               { return false; }
bool AudioVideoLoaderProcessor::producesMidi() const                              { return false; }
bool AudioVideoLoaderProcessor::isMidiEffect() const                              { return false; }
double AudioVideoLoaderProcessor::getTailLengthSeconds() const                    { return 0.0; }
int AudioVideoLoaderProcessor::getNumPrograms()                                   { return 1; }
int AudioVideoLoaderProcessor::getCurrentProgram()                                { return 0; }
void AudioVideoLoaderProcessor::setCurrentProgram (int)                           {}
const juce::String AudioVideoLoaderProcessor::getProgramName (int)                { return {}; }
void AudioVideoLoaderProcessor::changeProgramName (int, const juce::String&)      {}

void AudioVideoLoaderProcessor::prepareToPlay (double sampleRate, int)
{
    currentHostSampleRate.store (sampleRate);
}

void AudioVideoLoaderProcessor::releaseResources() {}

#if ! JucePlugin_IsMidiEffect
bool AudioVideoLoaderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    if (in != out)
        return false;

    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}
#endif

std::shared_ptr<AudioVideoLoaderProcessor::LoadedAudioData> AudioVideoLoaderProcessor::getLoadedAudioSnapshot() const
{
    std::scoped_lock lock (audioMutex);
    return loadedAudio;
}

float AudioVideoLoaderProcessor::readSampleLinear (const LoadedAudioData& file, int channel, double sourceIndex) const
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
    return data[i0] + frac * (data[i1] - data[i0]);
}

double AudioVideoLoaderProcessor::wrapSourceIndex (double sourceIndex, int numSamples) const
{
    if (numSamples <= 0)
        return 0.0;

    auto wrapped = std::fmod (sourceIndex, static_cast<double> (numSamples));
    if (wrapped < 0.0)
        wrapped += static_cast<double> (numSamples);
    return wrapped;
}

void AudioVideoLoaderProcessor::setTransportSummary (const juce::String& text)
{
    std::scoped_lock lock (transportMutex);
    transportSummary = text;
}

void AudioVideoLoaderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto* audioPlayHead = getPlayHead();
    if (audioPlayHead == nullptr)
    {
        hostIsPlaying.store (false);
        setTransportSummary ("Ingen host-transport");
        return;
    }

    const auto position = audioPlayHead->getPosition();
    if (! position.hasValue())
    {
        hostIsPlaying.store (false);
        setTransportSummary ("Ingen positionsdata");
        return;
    }

    const auto playingNow = position->getIsPlaying();
    hostIsPlaying.store (playingNow);

    const auto hostRate = juce::jmax (1.0, currentHostSampleRate.load());

    if (const auto timeInSeconds = position->getTimeInSeconds(); timeInSeconds.hasValue())
        currentHostTimeSeconds.store (*timeInSeconds);
    else if (const auto timeInSamples = position->getTimeInSamples(); timeInSamples.hasValue())
        currentHostTimeSeconds.store (static_cast<double> (*timeInSamples) / hostRate);
    else
        currentHostTimeSeconds.store (0.0);

    if (! playingNow)
    {
        setTransportSummary (hasLoadedAudio() ? "Stoppad • ljudfil redo" : "Stoppad");
        return;
    }

    const auto timeInSamples = position->getTimeInSamples();
    if (! timeInSamples.hasValue())
    {
        setTransportSummary ("Spelar • men hosten gav ingen sample-position");
        return;
    }

    const auto audio = getLoadedAudioSnapshot();
    if (audio == nullptr || audio->buffer.getNumChannels() <= 0 || audio->buffer.getNumSamples() <= 0)
    {
        setTransportSummary (hasLoadedVideo() ? "Spelar • video laddad, ingen ljudfil" : "Spelar • ingen ljudfil");
        return;
    }

    const auto hostStartSample = static_cast<double> (*timeInSamples);
    const auto ratio = audio->sampleRate / hostRate;
    const auto shouldLoop = loopEnabled.load();
    const auto gain = gainLinear.load();
    const auto fileChannels = audio->buffer.getNumChannels();

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto sourceChannel = juce::jmin (channel, fileChannels - 1);
        auto* out = buffer.getWritePointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto sourceIndex = (hostStartSample + static_cast<double> (sample)) * ratio;

            if (shouldLoop)
                sourceIndex = wrapSourceIndex (sourceIndex, audio->buffer.getNumSamples());
            else if (sourceIndex >= static_cast<double> (audio->buffer.getNumSamples()))
                continue;

            out[sample] += gain * readSampleLinear (*audio, sourceChannel, sourceIndex);
        }
    }

    auto summary = shouldLoop ? juce::String ("Spelar • audio loop") : juce::String ("Spelar • audio synkad");
    if (hasLoadedVideo())
        summary << " • video laddad";
    setTransportSummary (summary);
}

bool AudioVideoLoaderProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioVideoLoaderProcessor::createEditor()
{
    return new AudioVideoLoaderEditor (*this);
}

void AudioVideoLoaderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("AudioVideoLoaderState");
    state.setProperty ("audioPath", getLoadedAudioPath(), nullptr);
    state.setProperty ("videoPath", getLoadedVideoPath(), nullptr);
    state.setProperty ("loopEnabled", isLoopEnabled(), nullptr);
    state.setProperty ("gainLinear", getGainLinear(), nullptr);

    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, destData);
}

void AudioVideoLoaderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
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
            clearLoadedAudio();
    }
    else
    {
        clearLoadedAudio();
    }

    const auto videoPath = state.getProperty ("videoPath").toString();
    if (videoPath.isNotEmpty())
    {
        const juce::File file (videoPath);
        if (file.existsAsFile())
            setVideoFile (file);
        else
            clearLoadedVideo();
    }
    else
    {
        clearLoadedVideo();
    }
}

bool AudioVideoLoaderProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->numChannels <= 0 || reader->lengthInSamples <= 0)
        return false;

    auto fresh = std::make_shared<LoadedAudioData>();
    fresh->buffer.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));

    if (! reader->read (&fresh->buffer,
                        0,
                        static_cast<int> (reader->lengthInSamples),
                        0,
                        true,
                        true))
        return false;

    fresh->sampleRate = reader->sampleRate;
    fresh->filePath = file.getFullPathName();
    fresh->fileName = file.getFileName();

    {
        std::scoped_lock lock (audioMutex);
        loadedAudio = std::move (fresh);
    }

    return true;
}

void AudioVideoLoaderProcessor::clearLoadedAudio()
{
    std::scoped_lock lock (audioMutex);
    loadedAudio.reset();
}

bool AudioVideoLoaderProcessor::hasLoadedAudio() const
{
    return getLoadedAudioSnapshot() != nullptr;
}

juce::String AudioVideoLoaderProcessor::getLoadedAudioName() const
{
    if (const auto audio = getLoadedAudioSnapshot())
        return audio->fileName;
    return "Ingen ljudfil laddad";
}

juce::String AudioVideoLoaderProcessor::getLoadedAudioPath() const
{
    if (const auto audio = getLoadedAudioSnapshot())
        return audio->filePath;
    return {};
}

juce::String AudioVideoLoaderProcessor::getSupportedAudioWildcard() const
{
    return formatManager.getWildcardForAllFormats();
}

bool AudioVideoLoaderProcessor::setVideoFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    std::scoped_lock lock (videoMutex);
    loadedVideoPath = file.getFullPathName();
    loadedVideoName = file.getFileName();
    return true;
}

void AudioVideoLoaderProcessor::clearLoadedVideo()
{
    std::scoped_lock lock (videoMutex);
    loadedVideoPath.clear();
    loadedVideoName.clear();
}

bool AudioVideoLoaderProcessor::hasLoadedVideo() const
{
    std::scoped_lock lock (videoMutex);
    return loadedVideoPath.isNotEmpty();
}

juce::String AudioVideoLoaderProcessor::getLoadedVideoName() const
{
    std::scoped_lock lock (videoMutex);
    return loadedVideoName.isNotEmpty() ? loadedVideoName : "Ingen videofil laddad";
}

juce::String AudioVideoLoaderProcessor::getLoadedVideoPath() const
{
    std::scoped_lock lock (videoMutex);
    return loadedVideoPath;
}

juce::String AudioVideoLoaderProcessor::getSupportedVideoWildcard() const
{
    return "*.mp4;*.mov;*.m4v;*.avi;*.wmv;*.mkv";
}

void AudioVideoLoaderProcessor::setLoopEnabled (bool shouldLoop)
{
    loopEnabled.store (shouldLoop);
}

bool AudioVideoLoaderProcessor::isLoopEnabled() const
{
    return loopEnabled.load();
}

void AudioVideoLoaderProcessor::setGainLinear (float newGain)
{
    gainLinear.store (juce::jlimit (0.0f, 2.0f, newGain));
}

float AudioVideoLoaderProcessor::getGainLinear() const
{
    return gainLinear.load();
}

double AudioVideoLoaderProcessor::getCurrentHostTimeSeconds() const
{
    return currentHostTimeSeconds.load();
}

bool AudioVideoLoaderProcessor::getHostIsPlaying() const
{
    return hostIsPlaying.load();
}

juce::String AudioVideoLoaderProcessor::getTransportSummary() const
{
    std::scoped_lock lock (transportMutex);
    return transportSummary;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioVideoLoaderProcessor();
}
