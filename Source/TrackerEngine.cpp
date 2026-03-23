#include "TrackerEngine.h"

//==============================================================================
TrackerEngine::TrackerEngine()
{
    for (int i = 0; i < kNumTracks; ++i)
        tracks[i].name = "Voice " + juce::String (i + 1);
}

void TrackerEngine::prepare (double sr)
{
    sampleRate = sr;
    recalcSamplesPerStep();
    sampleAccum = 0.0;
}

void TrackerEngine::reset()
{
    for (auto& t : tracks)
        t.curStep = juce::jmax (0, t.stepCount - 1);
    // Pre-arm so the very first advance() call fires step 0 immediately
    // rather than waiting one full step duration before the first beat.
    sampleAccum = samplesPerStep;
}

void TrackerEngine::setPlaying (bool p)
{
    if (p && !playing.load())
    {
        // Start one step back so first advance lands on step 0
        for (auto& t : tracks)
            t.curStep = juce::jmax (0, t.stepCount - 1);
        sampleAccum = samplesPerStep;
    }
    playing.store (p);
}

void TrackerEngine::setBpm (double b)
{
    bpm.store (juce::jlimit (20.0, 300.0, b));
    recalcSamplesPerStep();
}

//==============================================================================
void TrackerEngine::resetAll()
{
    for (int i = 0; i < kNumTracks; ++i)
    {
        auto& track = tracks[i];

        // Clear all steps
        for (auto& step : track.steps)
            step = TrackerStep{};

        // Reset track-level settings (keep voice name)
        track.params    = FMVoiceParams{};
        track.stepCount = 16;
        track.muted     = false;
        track.curStep   = 0;

        // Cancel any active stutter and pending audio events
        track.stutterActive    = false;
        track.stutterRemaining = 0;
        track.stutterAccum     = 0.0;
        track.hasPending.store    (false);
        track.pendingOff.store    (false);
        track.pendingSlide.store  (false);
    }

    playing.store (false);
    sampleAccum = 0.0;
}

void TrackerEngine::recalcSamplesPerStep()
{
    // One 16th note = (60 / bpm) / 4 seconds
    samplesPerStep = (sampleRate * 60.0) / (bpm.load() * 4.0);
}

void TrackerEngine::syncToHostPosition (double ppqPos, double bpmVal, double sr)
{
    sampleRate = sr;
    bpm.store (juce::jlimit (20.0, 300.0, bpmVal));
    recalcSamplesPerStep();

    // Convert PPQ position to 16th-note grid position.
    // 1 sixteenth note = 0.25 quarter notes.
    const double totalSixteenths = ppqPos / 0.25;
    const auto   globalStep      = (long long) totalSixteenths;           // complete 16ths elapsed
    const double fraction        = totalSixteenths - (double) globalStep; // 0.0..1.0 within step

    if (fraction < 0.01)
    {
        // Right on (or within 1%) of a step boundary — fire this step immediately.
        for (auto& t : tracks)
        {
            const int sc = juce::jmax (1, t.stepCount);
            t.curStep = (int) (((globalStep - 1) % sc + sc) % sc);
        }
        sampleAccum = samplesPerStep;   // pre-armed: fires on very next advance()
    }
    else
    {
        // Mid-step: current step already fired; wait out the remaining fraction.
        for (auto& t : tracks)
        {
            const int sc = juce::jmax (1, t.stepCount);
            t.curStep = (int) ((globalStep % sc + sc) % sc);
        }
        sampleAccum = fraction * samplesPerStep;
    }

    playing.store (true);
}

//==============================================================================
void TrackerEngine::advance (int numSamples, bool hostIsPlaying, double hostBpm)
{
    if (!playing.load() && !hostIsPlaying)
        return;

    // Sync BPM to host if host is playing.
    // setBpm() calls recalcSamplesPerStep() internally, so no redundant call needed.
    if (hostIsPlaying && hostBpm > 0.0)
        setBpm (hostBpm);

    // ---- Fire stutter retriggers that fall within this buffer ----
    // These use their own accumulator relative to when the step first fired.
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& track = tracks[t];
        if (!track.stutterActive || track.stutterRemaining <= 0)
            continue;

        track.stutterAccum += numSamples;

        while (track.stutterAccum >= track.stutterPeriod && track.stutterRemaining > 0)
        {
            track.stutterAccum -= track.stutterPeriod;
            --track.stutterRemaining;
            track.pendingNote.store      (track.stutterNote);
            track.pendingVel.store       (track.stutterVel);
            track.pendingIsStutter.store (true);   // skip noteOff to avoid click
            track.pendingSampleOffset    = 0;       // fires at start of current buffer
            track.hasPending.store       (true);
        }
    }

    // ---- Advance step boundaries ----
    // Record accumulator before adding so we can compute the exact sample
    // offset within this buffer where each step boundary falls.
    const double accumBefore = sampleAccum;
    sampleAccum += numSamples;

    // Samples from the start of this buffer until the first step threshold.
    double samplesUntilStep = samplesPerStep - accumBefore;

    while (sampleAccum >= samplesPerStep)
    {
        sampleAccum -= samplesPerStep;

        // Exact sample within this buffer where the step fires (clamped to valid range).
        const int stepOffset = juce::jlimit (0, numSamples - 1,
                                             juce::roundToInt (samplesUntilStep));

        // A new step fires: cancel any remaining stutter from the previous step,
        // advance each track independently (polymeter), then fire the new step.
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto& track = tracks[t];
            track.stutterActive       = false;  // stutter is step-local
            track.stutterRemaining    = 0;
            track.stutterAccum        = 0.0;
            track.pendingSampleOffset = stepOffset;
            track.curStep = (track.curStep + 1) % juce::jmax (1, track.stepCount);
            fireStep (t);
        }

        samplesUntilStep += samplesPerStep;  // next step is one period later

        // Write display snapshot AFTER all tracks have moved so the UI
        // always sees every column at the same logical step boundary
        for (int t = 0; t < kNumTracks; ++t)
            displaySteps[t].store (tracks[t].curStep, std::memory_order_relaxed);
    }
}

void TrackerEngine::fireStep (int trackIdx)
{
    auto& track = tracks[trackIdx];

    if (track.muted)
        return;

    const auto& step = track.steps[track.curStep];

    if (step.note > 0)
    {
        if (step.slide)
        {
            // Portamento: keep the voice playing, bend pitch to the new note.
            const int slideSamples = juce::jmax (1,
                (int)(samplesPerStep * juce::jlimit (0.0f, 1.0f, step.slideLen)));
            track.pendingSlideNote.store    ((int8_t)  step.note);
            track.pendingSlideVel.store     ((uint8_t) step.vel);
            track.pendingSlideSamples.store (slideSamples);
            track.pendingSlide.store        (true);
            // Slide steps never stutter
            track.stutterActive    = false;
            track.stutterRemaining = 0;
        }
        else
        {
            track.pendingNote.store       (step.note);
            track.pendingVel.store        (step.vel);
            track.pendingIsStutter.store  (false);  // main step, do full noteOff+noteOn
            track.hasPending.store        (true);

            // Set up intra-step stutter retriggers if active
            if (step.stutter && step.stutterCount > 1)
            {
                track.stutterActive    = true;
                track.stutterRemaining = step.stutterCount - 1;  // first already fired
                track.stutterAccum     = 0.0;
                track.stutterPeriod    = samplesPerStep / step.stutterCount;
                track.stutterNote      = step.note;
                track.stutterVel       = step.vel;
            }
            else
            {
                track.stutterActive    = false;
                track.stutterRemaining = 0;
            }
        }
    }
    else
    {
        track.stutterActive    = false;
        track.stutterRemaining = 0;
        track.pendingOff.store (true);
    }
}

//==============================================================================
juce::ValueTree TrackerEngine::toValueTree() const
{
    juce::ValueTree root ("TrackerEngine");
    root.setProperty ("bpm", bpm.load(), nullptr);
    root.setProperty ("playing", playing.load(), nullptr);

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto& track = tracks[t];
        juce::ValueTree vt ("Track");
        vt.setProperty ("index",     t,                  nullptr);
        vt.setProperty ("stepCount", track.stepCount,    nullptr);
        vt.setProperty ("muted",     track.muted,        nullptr);
        vt.setProperty ("name",      track.name,         nullptr);

        // FM params
        const auto& p = track.params;
        vt.setProperty ("cAttack",      p.cAttack,      nullptr);
        vt.setProperty ("cDecay",       p.cDecay,       nullptr);
        vt.setProperty ("cSustain",     p.cSustain,     nullptr);
        vt.setProperty ("cRelease",     p.cRelease,     nullptr);
        vt.setProperty ("mAttack",      p.mAttack,      nullptr);
        vt.setProperty ("mDecay",       p.mDecay,       nullptr);
        vt.setProperty ("mSustain",     p.mSustain,     nullptr);
        vt.setProperty ("mRelease",     p.mRelease,     nullptr);
        vt.setProperty ("modRatio",     p.modRatio,     nullptr);
        vt.setProperty ("modIndex",     p.modIndex,     nullptr);
        vt.setProperty ("bitDepth",     p.bitDepth,     nullptr);
        vt.setProperty ("srDivisor",    p.srDivisor,    nullptr);
        vt.setProperty ("filterCutoff", p.filterCutoff, nullptr);
        vt.setProperty ("filterIsLP",   p.filterIsLP,   nullptr);
        vt.setProperty ("volume",       p.volume,       nullptr);

        // Steps
        for (int s = 0; s < kMaxSteps; ++s)
        {
            const auto& step = track.steps[s];
            juce::ValueTree sv ("Step");
            sv.setProperty ("n",   (int)step.note,          nullptr);
            sv.setProperty ("v",   (int)step.vel,           nullptr);
            sv.setProperty ("f",   (int)step.fx,            nullptr);
            sv.setProperty ("p",   (int)step.fxVal,         nullptr);
            sv.setProperty ("sl",  step.slide ? 1 : 0,      nullptr);
            sv.setProperty ("sll", step.slideLen,            nullptr);
            sv.setProperty ("st",  step.stutter ? 1 : 0,    nullptr);
            sv.setProperty ("stc", step.stutterCount,        nullptr);
            vt.addChild (sv, -1, nullptr);
        }

        root.addChild (vt, -1, nullptr);
    }

    return root;
}

void TrackerEngine::fromValueTree (const juce::ValueTree& root)
{
    if (!root.isValid() || root.getType().toString() != "TrackerEngine")
        return;

    bpm.store     ((double) root.getProperty ("bpm",     120.0));
    playing.store ((bool)   root.getProperty ("playing", false));

    for (auto trackVT : root)
    {
        if (trackVT.getType().toString() != "Track") continue;
        int idx = (int) trackVT.getProperty ("index", -1);
        if (idx < 0 || idx >= kNumTracks) continue;

        auto& track = tracks[idx];
        track.stepCount = (int)    trackVT.getProperty ("stepCount", 16);
        track.muted     = (bool)   trackVT.getProperty ("muted",     false);
        track.name      = trackVT.getProperty ("name",   track.name).toString();

        auto& p = track.params;
        p.cAttack      = (float) trackVT.getProperty ("cAttack",      0.001f);
        p.cDecay       = (float) trackVT.getProperty ("cDecay",       0.25f);
        p.cSustain     = (float) trackVT.getProperty ("cSustain",     0.0f);
        p.cRelease     = (float) trackVT.getProperty ("cRelease",     0.05f);
        p.mAttack      = (float) trackVT.getProperty ("mAttack",      0.001f);
        p.mDecay       = (float) trackVT.getProperty ("mDecay",       0.12f);
        p.mSustain     = (float) trackVT.getProperty ("mSustain",     0.0f);
        p.mRelease     = (float) trackVT.getProperty ("mRelease",     0.05f);
        p.modRatio     = (float) trackVT.getProperty ("modRatio",     2.0f);
        p.modIndex     = (float) trackVT.getProperty ("modIndex",     5.0f);
        p.bitDepth     = (float) trackVT.getProperty ("bitDepth",     8.0f);
        p.srDivisor    = (int)   trackVT.getProperty ("srDivisor",    1);
        p.filterCutoff = (float) trackVT.getProperty ("filterCutoff", 0.5f);
        p.filterIsLP   = (bool)  trackVT.getProperty ("filterIsLP",   true);
        p.volume       = (float) trackVT.getProperty ("volume",       0.8f);

        int s = 0;
        for (auto stepVT : trackVT)
        {
            if (stepVT.getType().toString() != "Step" || s >= kMaxSteps) continue;
            track.steps[s].note         = (int8_t)  (int) stepVT.getProperty ("n",   0);
            track.steps[s].vel          = (uint8_t) (int) stepVT.getProperty ("v",   100);
            track.steps[s].fx           = (uint8_t) (int) stepVT.getProperty ("f",   0);
            track.steps[s].fxVal        = (uint8_t) (int) stepVT.getProperty ("p",   0);
            track.steps[s].slide        = (int) stepVT.getProperty ("sl",  0) != 0;
            track.steps[s].slideLen     = (float) stepVT.getProperty ("sll", 0.5f);
            track.steps[s].stutter      = (int) stepVT.getProperty ("st",  0) != 0;
            track.steps[s].stutterCount = juce::jlimit (1, 4, (int) stepVT.getProperty ("stc", 2));
            ++s;
        }
    }

    recalcSamplesPerStep();
}
