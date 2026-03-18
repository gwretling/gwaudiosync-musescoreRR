#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <memory>
#include <mutex>

class AudioVideoLoaderProcessor : public juce::AudioProcessor
{
public:
    AudioVideoLoaderProcessor();
    ~AudioVideoLoaderProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #if ! JucePlugin_IsMidiEffect
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool loadAudioFile (const juce::File& file);
    void clearLoadedAudio();
    bool hasLoadedAudio() const;
    juce::String getLoadedAudioName() const;
    juce::String getLoadedAudioPath() const;
    juce::String getSupportedAudioWildcard() const;

    bool setVideoFile (const juce::File& file);
    void clearLoadedVideo();
    bool hasLoadedVideo() const;
    juce::String getLoadedVideoName() const;
    juce::String getLoadedVideoPath() const;
    juce::String getSupportedVideoWildcard() const;

    void setLoopEnabled (bool shouldLoop);
    bool isLoopEnabled() const;

    void setGainLinear (float newGain);
    float getGainLinear() const;

    double getCurrentHostTimeSeconds() const;
    bool getHostIsPlaying() const;
    juce::String getTransportSummary() const;

private:
    struct LoadedAudioData
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
        juce::String filePath;
        juce::String fileName;
    };

    std::shared_ptr<LoadedAudioData> getLoadedAudioSnapshot() const;
    float readSampleLinear (const LoadedAudioData& file, int channel, double sourceIndex) const;
    double wrapSourceIndex (double sourceIndex, int numSamples) const;
    void setTransportSummary (const juce::String& text);

    juce::AudioFormatManager formatManager;

    mutable std::mutex audioMutex;
    std::shared_ptr<LoadedAudioData> loadedAudio;

    mutable std::mutex videoMutex;
    juce::String loadedVideoPath;
    juce::String loadedVideoName;

    mutable std::mutex transportMutex;
    juce::String transportSummary { "Stoppad" };

    std::atomic<double> currentHostSampleRate { 44100.0 };
    std::atomic<double> currentHostTimeSeconds { 0.0 };
    std::atomic<bool> hostIsPlaying { false };
    std::atomic<float> gainLinear { 1.0f };
    std::atomic<bool> loopEnabled { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioVideoLoaderProcessor)
};
