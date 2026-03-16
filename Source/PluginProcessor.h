#pragma once
#include <JuceHeader.h>
#include "TrackerEngine.h"
#include "FMVoice.h"

//==============================================================================
class LoFiTrackerAudioProcessor : public juce::AudioProcessor
{
public:
    LoFiTrackerAudioProcessor();
    ~LoFiTrackerAudioProcessor() override;

    //==========================================================================
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool  acceptsMidi()   const override { return true; }
    bool  producesMidi()  const override { return false; }
    bool  isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Public interface for the editor
    TrackerEngine&       getEngine()       { return engine; }
    const TrackerEngine& getEngine() const { return engine; }

    // Called from UI thread — safe because we use atomics for handoff
    void setInternalPlaying (bool p) { engine.setPlaying (p); }
    void setInternalBpm     (double b) { engine.setBpm (b); }

private:
    TrackerEngine engine;
    FMVoice       voices[kNumTracks];

    // Scratch buffer for voice rendering
    juce::AudioBuffer<float> scratchBuffer;

    bool lastHostPlaying { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiTrackerAudioProcessor)
};
