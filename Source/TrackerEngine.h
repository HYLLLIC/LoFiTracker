#pragma once
#include <JuceHeader.h>

//==============================================================================
// Maximum steps per track
static constexpr int kMaxSteps    = 64;
static constexpr int kNumTracks   = 5;

//==============================================================================
// A single step in the pattern. note==0 means empty/off.
struct TrackerStep
{
    int8_t  note         = 0;      // 0 = empty, 1..127 = MIDI note
    uint8_t vel          = 100;    // 0..127
    uint8_t fx           = 0;      // Effect command (reserved for future use)
    uint8_t fxVal        = 0;      // Effect value

    bool    slide        = false;  // Portamento glide into next note
    float   slideLen     = 0.5f;   // Slide duration: 0.0..1.0 (fraction of step)

    bool    stutter      = false;  // Retrigger the note within the step
    int     stutterCount = 2;      // Number of retriggles: 1..4
};

//==============================================================================
// FM voice parameters stored per-track
struct FMVoiceParams
{
    // Carrier envelope
    float cAttack   = 0.001f;   // seconds
    float cDecay    = 0.25f;
    float cSustain  = 0.0f;
    float cRelease  = 0.05f;

    // Modulator envelope
    float mAttack   = 0.001f;
    float mDecay    = 0.12f;
    float mSustain  = 0.0f;
    float mRelease  = 0.05f;

    // FM parameters
    float modRatio  = 2.0f;     // modulator freq = carrier * modRatio
    float modIndex  = 5.0f;     // FM depth

    // Lo-fi
    float bitDepth  = 8.0f;     // 1..16 bits
    int   srDivisor = 1;        // sample rate = native / srDivisor (1..8)

    // Filter (simple 1-pole)
    float filterCutoff = 0.5f;  // 0..1 normalised
    bool  filterIsLP   = true;

    // Output
    float volume    = 0.8f;
};

//==============================================================================
struct TrackerTrack
{
    TrackerStep steps[kMaxSteps];
    int         stepCount  = 16;    // active loop length
    int         curStep    = 0;     // current playback position
    bool        muted      = false;
    juce::String name;
    FMVoiceParams params;

    // Pending events for the audio thread
    std::atomic<int>     pendingNote  { 0 };    // 0 = none
    std::atomic<uint8_t> pendingVel   { 0 };
    std::atomic<bool>    hasPending   { false };
    std::atomic<bool>    pendingOff   { false }; // empty step hit — stop voice

    // Pending slide (portamento/glide) event — audio thread only via processBlock.
    // Set by fireStep() when step.slide is true; consumed by the processor.
    std::atomic<bool>    pendingSlide        { false };
    std::atomic<int8_t>  pendingSlideNote    { 0 };
    std::atomic<uint8_t> pendingSlideVel     { 0 };
    std::atomic<int>     pendingSlideSamples { 0 };

    // Exact sample offset within the current processBlock buffer where this
    // step's note-on should fire.  Written by advance(), read by processBlock().
    // Audio-thread only — no atomic needed.
    int pendingSampleOffset { 0 };

    // Stutter retrigger state — audio thread only, no atomics needed.
    // Set up by fireStep() when a stutter step fires; advanced by advance().
    bool    stutterActive    { false };
    int     stutterRemaining { 0 };      // retriggers still to fire
    double  stutterAccum     { 0.0 };    // sample accumulator for retrigger timing
    double  stutterPeriod    { 0.0 };    // samples between retriggers
    int8_t  stutterNote      { 0 };
    uint8_t stutterVel       { 0 };

    TrackerTrack() = default;
};

//==============================================================================
class TrackerEngine
{
public:
    TrackerEngine();

    void prepare (double sampleRate);
    void reset();

    // Called from processBlock; advances sequencer, fires note-ons
    // Returns bitmask of tracks that fired this block (for audio thread)
    void advance (int numSamples, bool hostIsPlaying, double hostBpm);

    bool   isPlaying()  const { return playing.load(); }
    void   setPlaying  (bool p);
    void   setBpm      (double b);
    double getBpm()    const { return bpm.load(); }

    // Sync engine phase to host PPQ position (call on host play-start).
    // Sets sampleAccum so step boundaries align to the host's 16th-note grid,
    // then starts playing.  Do NOT call setPlaying(true) afterwards.
    void syncToHostPosition (double ppqPos, double bpmVal, double sr);

    // Clears all notes and resets every track to defaults; keeps BPM.
    void resetAll();

    // Accessors for UI thread (use with care — non-atomic step data)
    TrackerTrack& getTrack (int idx) { jassert(idx >= 0 && idx < kNumTracks); return tracks[idx]; }
    const TrackerTrack& getTrack (int idx) const { return tracks[idx]; }

    // Returns the display-safe step position for track (written atomically
    // after ALL tracks advance — UI reads this, never track.curStep directly)
    int getDisplayStep (int trackIdx) const
    {
        jassert (trackIdx >= 0 && trackIdx < kNumTracks);
        return displaySteps[trackIdx].load (std::memory_order_relaxed);
    }

    // State serialisation
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& vt);

private:
    TrackerTrack tracks[kNumTracks];

    // Snapshot written by audio thread after ALL tracks advance; read by UI
    std::atomic<int> displaySteps[kNumTracks] {};

    std::atomic<bool>   playing        { false };
    std::atomic<double> bpm            { 120.0 };
    double              sampleRate     { 44100.0 };
    double              sampleAccum    { 0.0 };
    double              samplesPerStep { 0.0 };  // 16th note

    void fireStep (int trackIdx);
    void recalcSamplesPerStep();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackerEngine)
};
