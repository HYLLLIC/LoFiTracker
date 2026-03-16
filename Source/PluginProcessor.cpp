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

    // ---- Detect play → stop transition and release all voices ----
    const bool isNowPlaying = engine.isPlaying() || hostPlaying;
    if (lastHostPlaying && !isNowPlaying)
        for (auto& v : voices)
            v.noteOff();
    lastHostPlaying = isNowPlaying;

    // ---- Advance sequencer ----
    engine.advance (buffer.getNumSamples(), hostPlaying, hostBpm);

    // ---- Consume pending note-ons and render voices ----
    scratchBuffer.setSize (2, buffer.getNumSamples(), false, false, true);
    scratchBuffer.clear();

    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& track = engine.getTrack (t);
        auto& voice = voices[t];

        if (track.hasPending.load())
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
