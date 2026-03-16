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
    sampleAccum = 0.0;
}

void TrackerEngine::setPlaying (bool p)
{
    if (p && !playing.load())
    {
        // Start one step back so first advance lands on step 0
        for (auto& t : tracks)
            t.curStep = juce::jmax (0, t.stepCount - 1);
        sampleAccum = 0.0;
    }
    playing.store (p);
}

void TrackerEngine::setBpm (double b)
{
    bpm.store (juce::jlimit (20.0, 300.0, b));
    recalcSamplesPerStep();
}

void TrackerEngine::recalcSamplesPerStep()
{
    // One 16th note = (60 / bpm) / 4 seconds
    samplesPerStep = (sampleRate * 60.0) / (bpm.load() * 4.0);
}

//==============================================================================
void TrackerEngine::advance (int numSamples, bool hostIsPlaying, double hostBpm)
{
    if (!playing.load() && !hostIsPlaying)
        return;

    // Sync BPM to host if host is playing
    if (hostIsPlaying && hostBpm > 0.0)
        setBpm (hostBpm);

    recalcSamplesPerStep();

    sampleAccum += numSamples;

    while (sampleAccum >= samplesPerStep)
    {
        sampleAccum -= samplesPerStep;

        // Advance each track independently (polymeter)
        // Advance first, then fire — curStep always shows the sounding step
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto& track = tracks[t];
            track.curStep = (track.curStep + 1) % juce::jmax (1, track.stepCount);
            fireStep (t);
        }
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
        track.pendingNote.store  (step.note);
        track.pendingVel.store   (step.vel);
        track.hasPending.store   (true);
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
            sv.setProperty ("n", (int)step.note,   nullptr);
            sv.setProperty ("v", (int)step.vel,    nullptr);
            sv.setProperty ("f", (int)step.fx,     nullptr);
            sv.setProperty ("p", (int)step.fxVal,  nullptr);
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
            track.steps[s].note   = (int8_t)  (int) stepVT.getProperty ("n", 0);
            track.steps[s].vel    = (uint8_t) (int) stepVT.getProperty ("v", 100);
            track.steps[s].fx     = (uint8_t) (int) stepVT.getProperty ("f", 0);
            track.steps[s].fxVal  = (uint8_t) (int) stepVT.getProperty ("p", 0);
            ++s;
        }
    }

    recalcSamplesPerStep();
}
