#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <mutex>

class TrackSyncedAudioLoaderAudioProcessor : public juce::AudioProcessor
{
public:
    TrackSyncedAudioLoaderAudioProcessor();
    ~TrackSyncedAudioLoaderAudioProcessor() override;

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
    void clearLoadedFile();

    juce::String getLoadedFileName() const;
    juce::String getLoadedFilePath() const;
    juce::String getSupportedWildcard() const;
    bool hasLoadedFile() const;

    void setLoopEnabled (bool shouldLoop);
    bool isLoopEnabled() const;

    void setGainLinear (float newGain);
    float getGainLinear() const;

    juce::String getTransportSummary() const;

private:
    struct LoadedFileData
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
        juce::String filePath;
        juce::String fileName;
    };

    float readSampleLinear (const LoadedFileData& file, int channel, double sourceIndex) const;
    std::shared_ptr<LoadedFileData> getLoadedFileSnapshot() const;
    double wrapSourceIndex (double sourceIndex, int numSamples) const;

    juce::AudioFormatManager formatManager;
    mutable std::mutex fileMutex;
    std::shared_ptr<LoadedFileData> loadedFile;

    std::atomic<double> currentHostSampleRate { 44100.0 };
    std::atomic<float> gainLinear { 1.0f };
    std::atomic<bool> loopEnabled { false };

    juce::String transportSummary { "Stoppad" };
    mutable std::mutex transportMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackSyncedAudioLoaderAudioProcessor)
};
