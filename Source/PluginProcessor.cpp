#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LoFiTrackerAudioProcessor::LoFiTrackerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

LoFiTrackerAudioProcessor::~LoFiTrackerAudioProcessor() {}

//==============================================================================
void LoFiTrackerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);

    for (auto& v : voices)
        v.prepare (sampleRate, samplesPerBlock);

    scratchBuffer.setSize (2, samplesPerBlock);
}

void LoFiTrackerAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LoFiTrackerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}
#endif

//==============================================================================
void LoFiTrackerAudioProcessor::resetAll()
{
    engine.resetAll();
    for (auto& v : voices)
        v.noteOff();
}

void LoFiTrackerAudioProcessor::setInternalPlaying (bool p)
{
    userStopped.store (! p, std::memory_order_relaxed);
    engine.setPlaying (p);
    if (! p)
        engine.reset();
}

//==============================================================================
void LoFiTrackerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // ---- Determine host transport state ----
    bool   hostPlaying = false;
    double hostBpm     = 0.0;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm())
                hostBpm = *bpm;
        }
    }

    // ---- Sync engine to host transport edges ----
    const bool hostStarted = hostPlaying  && ! lastRawHostPlaying;
    const bool hostStopped = ! hostPlaying &&   lastRawHostPlaying;
    lastRawHostPlaying = hostPlaying;

    if (hostStarted)
    {
        // Ableton pressed Play: sync engine phase to host PPQ position so
        // step boundaries align to the host's 16th-note grid exactly.
        userStopped.store (false, std::memory_order_relaxed);

        double ppqPos    = 0.0;
        double bpmAtStart = hostBpm > 0.0 ? hostBpm : 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto ppq = pos->getPpqPosition())
                    ppqPos = *ppq;

        engine.syncToHostPosition (ppqPos, bpmAtStart, getSampleRate());
        // syncToHostPosition sets playing = true internally; don't call setPlaying again.
    }
    else if (hostStopped)
    {
        // Ableton pressed Stop: silence and park
        engine.setPlaying (false);
    }

    // ---- Detect any engine play→stop transition (covers host AND standalone) ----
    const bool internalNowPlaying = engine.isPlaying();
    if (lastInternalPlaying && ! internalNowPlaying)
        for (auto& v : voices)
            v.noteOff();
    lastInternalPlaying = internalNowPlaying;

    // ---- Advance sequencer ----
    engine.advance (buffer.getNumSamples(), hostPlaying, hostBpm);

    // ---- Consume pending note-ons and render voices ----
    scratchBuffer.setSize (2, buffer.getNumSamples(), false, false, true);
    scratchBuffer.clear();

    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& track = engine.getTrack (t);
        auto& voice = voices[t];

        if (track.pendingSlide.load())
        {
            const int   note    = (int)   track.pendingSlideNote.load();
            const float vel     = (float) track.pendingSlideVel.load();
            const int   samples = track.pendingSlideSamples.load();
            track.pendingSlide.store (false);
            track.pendingOff.store   (false);  // superseded

            voice.glideTo (note, vel, samples, track.params);
        }
        else if (track.hasPending.load())
        {
            const int  note = track.pendingNote.load();
            const auto vel  = track.pendingVel.load();
            track.hasPending.store (false);
            track.pendingOff.store (false);  // superseded by note-on

            voice.noteOff();
            voice.noteOn (note, (float) vel, track.params);
        }
        else if (track.pendingOff.load())
        {
            track.pendingOff.store (false);
            voice.noteOff();
        }

        // Update any live param changes
        voice.applyParams (track.params);

        // Render into scratch
        voice.render (scratchBuffer, 0, buffer.getNumSamples());
    }

    // ---- Mix scratch into output ----
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        int srcCh = juce::jmin (ch, scratchBuffer.getNumChannels() - 1);
        buffer.addFrom (ch, 0, scratchBuffer, srcCh, 0, buffer.getNumSamples());
    }
}

//==============================================================================
juce::AudioProcessorEditor* LoFiTrackerAudioProcessor::createEditor()
{
    return new LoFiTrackerAudioProcessorEditor (*this);
}

//==============================================================================
void LoFiTrackerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto vt = engine.toValueTree();
    juce::MemoryOutputStream mos (destData, true);
    vt.writeToStream (mos);
}

void LoFiTrackerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, (size_t) sizeInBytes, false);
    auto vt = juce::ValueTree::readFromStream (mis);
    engine.fromValueTree (vt);
}

//==============================================================================
// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LoFiTrackerAudioProcessor();
}
