#include "FMVoice.h"

FMVoice::FMVoice() {}

//==============================================================================
void FMVoice::prepare (double sr, int /*blockSize*/)
{
    sampleRate = sr;
    carrierEnv.setSampleRate (sr);
    modEnv.setSampleRate     (sr);
    carrierPhase          = 0.0;
    modPhase              = 0.0;
    filterZ1              = 0.0f;
    srHoldCount           = 0;
    srHoldSample          = 0.0f;
    slideSamplesRemaining = 0;
    slideSamplesTotal     = 0;
    currentModRatio       = 2.0;
}

//==============================================================================
double FMVoice::midiNoteToHz (int note)
{
    return 440.0 * std::pow (2.0, (note - 69) / 12.0);
}

void FMVoice::recalcFilter (float normCutoff)
{
    // 1-pole IIR coefficient from normalised cutoff (0..1)
    const float cutoff = juce::jlimit (0.001f, 0.499f, normCutoff);
    // Bilinear approximation: coeff = exp(-2π * cutoff)
    filterCoeff = std::exp (-juce::MathConstants<float>::twoPi * cutoff);
}

//==============================================================================
void FMVoice::noteOn (int midiNote, float velocity, const FMVoiceParams& p)
{
    carrierFreq     = midiNoteToHz (juce::jlimit (0, 127, midiNote));
    currentModRatio = p.modRatio;
    modFreq         = carrierFreq * currentModRatio;
    modIndex        = p.modIndex;

    // Cancel any active portamento glide
    slideSamplesRemaining = 0;

    // Intentionally keep oscillator phases, filter state, and SR-hold state
    // across retriggers so the waveform stays continuous — this eliminates
    // clicks on stutter retriggers.  When the voice was fully idle the ADSR
    // envelope is at 0, so the starting phase has no audible effect anyway.

    currentVelocity = velocity / 127.0f;

    // Carrier envelope
    juce::ADSR::Parameters cp;
    cp.attack  = p.cAttack;
    cp.decay   = p.cDecay;
    cp.sustain = p.cSustain;
    cp.release = p.cRelease;
    carrierEnv.setParameters (cp);
    carrierEnv.noteOn();

    // Modulator envelope
    juce::ADSR::Parameters mp;
    mp.attack  = p.mAttack;
    mp.decay   = p.mDecay;
    mp.sustain = p.mSustain;
    mp.release = p.mRelease;
    modEnv.setParameters (mp);
    modEnv.noteOn();

    // Lo-fi settings
    bitDepth  = juce::jlimit (1.0f, 16.0f, p.bitDepth);
    srDivisor = juce::jlimit (1, 8, p.srDivisor);
    volume    = p.volume;

    recalcFilter (p.filterCutoff);
    filterIsLP = p.filterIsLP;
}

void FMVoice::noteOff()
{
    carrierEnv.noteOff();
    modEnv.noteOff();
    slideSamplesRemaining = 0;  // cancel any active glide
}

//==============================================================================
void FMVoice::glideTo (int targetNote, float velocity, int durationSamples,
                        const FMVoiceParams& p)
{
    const double targetFreq = midiNoteToHz (juce::jlimit (0, 127, targetNote));
    currentModRatio = p.modRatio;

    if (!isActive() || durationSamples <= 0)
    {
        // Voice was idle (or zero-length glide) — fall back to a normal note-on
        noteOn (targetNote, velocity, p);
        return;
    }

    // Voice is active: keep envelope running, just interpolate the pitch
    slideFromFreq         = carrierFreq;
    slideToFreq           = targetFreq;
    slideSamplesTotal     = durationSamples;
    slideSamplesRemaining = durationSamples;

    // Update synthesis params that may have changed (but don't retrigger envelope)
    modIndex   = p.modIndex;
    bitDepth   = juce::jlimit (1.0f, 16.0f, p.bitDepth);
    srDivisor  = juce::jlimit (1, 8, p.srDivisor);
    volume     = p.volume;
    recalcFilter (p.filterCutoff);
    filterIsLP = p.filterIsLP;
}

void FMVoice::applyParams (const FMVoiceParams& p)
{
    modIndex        = p.modIndex;
    currentModRatio = p.modRatio;
    // Don't overwrite modFreq mid-glide — render() updates it per-sample
    if (slideSamplesRemaining <= 0)
        modFreq = carrierFreq * currentModRatio;
    bitDepth  = juce::jlimit (1.0f, 16.0f, p.bitDepth);
    srDivisor = juce::jlimit (1, 8, p.srDivisor);
    volume    = p.volume;
    recalcFilter (p.filterCutoff);
    filterIsLP = p.filterIsLP;
}

//==============================================================================
float FMVoice::processSample (float input)
{
    // Bit crusher
    const float levels = std::pow (2.0f, bitDepth) - 1.0f;
    float s = std::round (input * levels) / levels;

    // 1-pole filter
    if (filterIsLP)
        filterZ1 = s * (1.0f - filterCoeff) + filterZ1 * filterCoeff;
    else
        filterZ1 = s * (1.0f - filterCoeff) + filterZ1 * filterCoeff;  // HP below

    if (filterIsLP)
        s = filterZ1;
    else
        s = s - filterZ1;   // high-pass = input - lowpass

    return s;
}

//==============================================================================
void FMVoice::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!carrierEnv.isActive())
        return;

    const double twoPi = juce::MathConstants<double>::twoPi;

    auto* L = buffer.getWritePointer (0, startSample);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- Portamento: interpolate carrier/mod frequencies per-sample ----
        if (slideSamplesRemaining > 0)
        {
            const double t = 1.0 - (double) slideSamplesRemaining / (double) slideSamplesTotal;
            carrierFreq = slideFromFreq + (slideToFreq - slideFromFreq) * t;
            modFreq     = carrierFreq * currentModRatio;
            --slideSamplesRemaining;
            if (slideSamplesRemaining == 0)
            {
                carrierFreq = slideToFreq;              // snap exactly at end
                modFreq     = slideToFreq * currentModRatio;
            }
        }

        const double carrierDelta = twoPi * carrierFreq / sampleRate;
        const double modDelta     = twoPi * modFreq     / sampleRate;

        // ---- Sample-rate reduction ----
        float outputSample;
        if (srDivisor > 1)
        {
            if (srHoldCount <= 0)
            {
                // Compute new sample at the reduced rate
                const float modEnvVal  = modEnv.getNextSample();
                const double modOutput = std::sin (modPhase) * (double)(modIndex * modEnvVal);

                modPhase += modDelta;
                if (modPhase > twoPi) modPhase -= twoPi;

                carrierPhase += carrierDelta;
                if (carrierPhase > twoPi) carrierPhase -= twoPi;

                const float carEnvVal  = carrierEnv.getNextSample();
                srHoldSample = (float)std::sin (carrierPhase + modOutput) * carEnvVal * currentVelocity * volume;
                srHoldSample = processSample (srHoldSample);
                srHoldCount  = srDivisor;
            }
            outputSample = srHoldSample;
            --srHoldCount;
        }
        else
        {
            // Full sample rate
            const float modEnvVal  = modEnv.getNextSample();
            const double modOutput = std::sin (modPhase) * (double)(modIndex * modEnvVal);

            modPhase     += modDelta;
            if (modPhase > twoPi) modPhase -= twoPi;

            carrierPhase += carrierDelta;
            if (carrierPhase > twoPi) carrierPhase -= twoPi;

            const float carEnvVal = carrierEnv.getNextSample();
            outputSample = (float)std::sin (carrierPhase + modOutput) * carEnvVal * currentVelocity * volume;
            outputSample = processSample (outputSample);
        }

        L[i] += outputSample;
        if (R != nullptr)
            R[i] += outputSample;
    }
}
