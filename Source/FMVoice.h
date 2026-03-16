#pragma once
#include <JuceHeader.h>
#include "TrackerEngine.h"

//==============================================================================
// 2-operator FM synthesiser voice with lo-fi post-processing.
// One instance per track. Not thread-safe — call only from audio thread.
class FMVoice
{
public:
    FMVoice();

    void prepare (double sampleRate, int blockSize);
    void noteOn  (int midiNote, float velocity, const FMVoiceParams& p);
    void noteOff ();
    void applyParams (const FMVoiceParams& p);  // update live params mid-voice

    // Render into buffer (adds to existing content)
    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    bool isActive() const { return carrierEnv.isActive(); }

private:
    double sampleRate { 44100.0 };

    // Oscillator phases
    double carrierPhase { 0.0 };
    double modPhase     { 0.0 };

    double carrierFreq  { 440.0 };
    double modFreq      { 880.0 };
    float  modIndex     { 5.0f };

    // ADSR envelopes
    juce::ADSR carrierEnv;
    juce::ADSR modEnv;

    // Lo-fi state
    float  bitDepth   { 8.0f };
    int    srDivisor  { 1 };
    float  srHoldSample { 0.0f };
    int    srHoldCount  { 0 };

    // Simple 1-pole filter
    float filterZ1    { 0.0f };
    float filterCoeff { 0.0f };
    bool  filterIsLP  { true };

    float volume      { 0.8f };
    float currentVelocity { 1.0f };

    void recalcFilter (float normCutoff);
    float processSample (float input);
    static double midiNoteToHz (int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMVoice)
};
