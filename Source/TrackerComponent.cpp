#include "TrackerComponent.h"

//==============================================================================
// Classic tracker note key map (like FastTracker 2)
//  Z=C  S=C# X=D  D=D# C=E  V=F  G=F# B=G  H=G# N=A  J=A# M=B
static const std::pair<int,int> kNoteKeys[] =
{
    { 'z', 0 }, { 's', 1 }, { 'x', 2 }, { 'd', 3 }, { 'c', 4 },
    { 'v', 5 }, { 'g', 6 }, { 'b', 7 }, { 'h', 8 }, { 'n', 9 },
    { 'j', 10}, { 'm', 11}
};

//==============================================================================
TrackerComponent::TrackerComponent (TrackerEngine& e)
    : engine (e)
{
    setWantsKeyboardFocus (true);
    startTimerHz (30);  // 30fps repaint for playhead
}

TrackerComponent::~TrackerComponent()
{
    stopTimer();
}

//==============================================================================
void TrackerComponent::timerCallback()
{
    repaint();
}

//==============================================================================
juce::String TrackerComponent::noteToString (int8_t note) const
{
    if (note <= 0) return "---";
    static const char* names[] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
    int n   = (note - 1) % 12;
    int oct = (note - 1) / 12;
    return juce::String (names[n]) + juce::String (oct);
}

juce::String TrackerComponent::velToString (uint8_t vel) const
{
    static char buf[4];
    snprintf (buf, sizeof(buf), "%02X", vel);
    return juce::String (buf);
}

//==============================================================================
juce::Rectangle<int> TrackerComponent::cellRect (int track, int row) const
{
    const int displayRow = row - scrollOffset;
    if (displayRow < 0 || displayRow >= visibleRows) return {};
    const int x = track * colW;
    const int y = headerH + displayRow * rowH;
    return { x, y, colW, rowH };
}

void TrackerComponent::scrollToCursor()
{
    ensureCursorVisible();
}

void TrackerComponent::ensureCursorVisible()
{
    if (selectedStep < scrollOffset)
        scrollOffset = selectedStep;
    else if (selectedStep >= scrollOffset + visibleRows)
        scrollOffset = selectedStep - visibleRows + 1;
    scrollOffset = juce::jmax (0, scrollOffset);
}

//==============================================================================
void TrackerComponent::resized()
{
    colW        = getWidth() / kNumTracks;
    visibleRows = juce::jmax (1, (getHeight() - headerH) / rowH);
}

//==============================================================================
void TrackerComponent::paint (juce::Graphics& g)
{
    g.fillAll (colBg);

    const int totalW = getWidth();

    // ---- Draw track columns ----
    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto& track = engine.getTrack (t);
        const int x = t * colW;
        const bool isSelected = (t == selectedTrack);

        // ---- Header ----
        {
            juce::Rectangle<int> hdr (x, 0, colW, headerH);
            g.setColour (isSelected ? colHeaderBg.brighter (0.05f) : colHeaderBg);
            g.fillRect (hdr);

            // Track name
            g.setColour (track.muted ? colMuted : colAccent);
            g.setFont (juce::Font ("Courier New", 13.0f, juce::Font::bold));
            g.drawText (track.name, x + 4, 2, colW - 8, 16, juce::Justification::centredLeft, false);

            // Step count controls: "LEN: [<] 16 [>]"
            const int btnW = 14;
            const int midY = 20;
            const juce::String lenStr = juce::String (track.stepCount);
            g.setColour (colText);
            g.setFont (juce::Font ("Courier New", 11.0f, juce::Font::plain));
            g.drawText ("LEN:",     x + 2, midY, 30, 14, juce::Justification::left, false);
            g.drawText ("[<]",      x + 33, midY, btnW, 14, juce::Justification::centred, false);
            g.drawText (lenStr,     x + 48, midY, 20, 14, juce::Justification::centred, false);
            g.drawText ("[>]",      x + 68, midY, btnW, 14, juce::Justification::centred, false);

            // Mute button
            g.setColour (track.muted ? colMuted : colDimText);
            g.drawText ("[M]", x + 4, 36, 30, 10, juce::Justification::left, false);
            g.setColour (colDimText);
        }

        // ---- Rows (steps) ----
        const int maxRow = juce::jmax (track.stepCount, 1);

        for (int row = scrollOffset; row < scrollOffset + visibleRows; ++row)
        {
            const juce::Rectangle<int> cell = cellRect (t, row);
            if (cell.isEmpty()) continue;

            const bool inPattern = (row < maxRow);
            const bool isPlay    = inPattern && (row == track.curStep) && engine.isPlaying();
            const bool isCursor  = (t == selectedTrack && row == selectedStep);

            // Background
            if (isCursor)
                g.setColour (colCursor);
            else if (isPlay)
                g.setColour (colPlayhead);
            else if (!inPattern)
                g.setColour (colBg.darker (0.1f));
            else if (row % 4 == 0)
                g.setColour (colRowGroup);
            else if (row % 2 == 0)
                g.setColour (colRowEven);
            else
                g.setColour (colRowOdd);

            g.fillRect (cell);

            if (!inPattern)
            {
                // Dim indicator for out-of-pattern rows
                g.setColour (colDimText.withAlpha (0.3f));
                g.drawText ("   --  --  --", cell.getX() + 4, cell.getY(), cell.getWidth() - 4, cell.getHeight(),
                            juce::Justification::centredLeft, false);
                continue;
            }

            // Step number
            g.setFont (juce::Font ("Courier New", 10.0f, juce::Font::plain));
            g.setColour (isPlay ? colAccent : (row % 4 == 0 ? colAccent.withAlpha(0.5f) : colStepNum));
            g.drawText (juce::String::formatted ("%02d", row),
                        cell.getX() + 2, cell.getY(), 20, cell.getHeight(),
                        juce::Justification::centredLeft, false);

            // Playhead arrow
            if (isPlay)
            {
                g.setColour (colAccent);
                g.drawText (">", cell.getX() + 2, cell.getY(), 8, cell.getHeight(),
                            juce::Justification::centredLeft, false);
            }

            // Step data
            const auto& step = track.steps[row];
            const bool  hasNote = (step.note > 0);

            g.setFont (juce::Font ("Courier New", 12.0f, hasNote ? juce::Font::bold : juce::Font::plain));
            g.setColour (hasNote ? colText : colDimText);

            juce::String cellText = noteToString (step.note);
            cellText += " ";
            cellText += hasNote ? velToString (step.vel) : "--";

            g.drawText (cellText, cell.getX() + 22, cell.getY(), cell.getWidth() - 24, cell.getHeight(),
                        juce::Justification::centredLeft, false);
        }

        // Column divider
        g.setColour (colDivider);
        g.drawVerticalLine (x + colW - 1, 0, (float) getHeight());
    }

    // Outer border
    g.setColour (colDivider);
    g.drawRect (getLocalBounds(), 1);

    // Header bottom line
    g.setColour (colAccent.withAlpha (0.4f));
    g.drawHorizontalLine (headerH - 1, 0.0f, (float) totalW);
}

//==============================================================================
void TrackerComponent::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    const int clickX = e.x;
    const int clickY = e.y;

    for (int t = 0; t < kNumTracks; ++t)
    {
        const int x = t * colW;
        if (clickX < x || clickX >= x + colW) continue;

        selectedTrack = t;

        if (clickY < headerH)
        {
            handleHeaderClick (t, { clickX - x, clickY });
        }
        else
        {
            const int row = (clickY - headerH) / rowH + scrollOffset;
            if (row >= 0 && row < kMaxSteps)
                selectedStep = row;
        }
        repaint();
        break;
    }
}

void TrackerComponent::handleHeaderClick (int trackIdx, const juce::Point<int>& local)
{
    auto& track = engine.getTrack (trackIdx);

    // Mute button: y 36..46, x 4..34
    if (local.y >= 36 && local.y <= 46 && local.x >= 4 && local.x <= 34)
    {
        track.muted = !track.muted;
        return;
    }

    // Step count [<] button: y 20..34, x 33..47
    if (local.y >= 20 && local.y <= 34)
    {
        if (local.x >= 33 && local.x <= 47)
        {
            track.stepCount = juce::jmax (1, track.stepCount - 1);
            if (onStepCountChanged) onStepCountChanged (trackIdx, track.stepCount);
        }
        // [>] button: x 68..82
        else if (local.x >= 68 && local.x <= 82)
        {
            track.stepCount = juce::jmin (kMaxSteps, track.stepCount + 1);
            if (onStepCountChanged) onStepCountChanged (trackIdx, track.stepCount);
        }
    }
}

//==============================================================================
int TrackerComponent::keyToNote (int keyCode) const
{
    const int lower = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) keyCode);
    for (auto& kn : kNoteKeys)
        if (kn.first == lower) return kn.second;
    return -1;
}

void TrackerComponent::enterNote (int semitone)
{
    const int midiNote = juce::jlimit (1, 127, currentOctave * 12 + semitone + 1);
    auto& step = engine.getTrack (selectedTrack).steps[selectedStep];
    step.note = (int8_t) midiNote;
    step.vel  = 100;

    // Advance cursor down
    const int maxStep = engine.getTrack (selectedTrack).stepCount;
    selectedStep = (selectedStep + 1) % juce::jmax (1, maxStep);
    ensureCursorVisible();
}

void TrackerComponent::clearStep()
{
    auto& step = engine.getTrack (selectedTrack).steps[selectedStep];
    step.note  = 0;
    step.vel   = 0;
    selectedStep = (selectedStep + 1) % juce::jmax (1, engine.getTrack (selectedTrack).stepCount);
    ensureCursorVisible();
}

void TrackerComponent::moveCursor (int dTrack, int dStep)
{
    selectedTrack = juce::jlimit (0, kNumTracks - 1, selectedTrack + dTrack);
    selectedStep  = juce::jlimit (0, kMaxSteps  - 1, selectedStep  + dStep);
    ensureCursorVisible();
}

//==============================================================================
bool TrackerComponent::keyPressed (const juce::KeyPress& key)
{
    const int kc = key.getKeyCode();

    // Octave: number keys 1-8
    if (kc >= '1' && kc <= '8')
    {
        currentOctave = kc - '0';
        return true;
    }

    // Note entry
    const int semitone = keyToNote (kc);
    if (semitone >= 0)
    {
        enterNote (semitone);
        return true;
    }

    // Delete / clear
    if (kc == juce::KeyPress::deleteKey || kc == juce::KeyPress::backspaceKey)
    {
        clearStep();
        return true;
    }

    // Navigation
    if (kc == juce::KeyPress::upKey)    { moveCursor (0, -1); return true; }
    if (kc == juce::KeyPress::downKey)  { moveCursor (0,  1); return true; }
    if (kc == juce::KeyPress::leftKey)  { moveCursor (-1, 0); return true; }
    if (kc == juce::KeyPress::rightKey) { moveCursor ( 1, 0); return true; }

    // Tab: next track
    if (kc == juce::KeyPress::tabKey)
    {
        moveCursor (key.getModifiers().isShiftDown() ? -1 : 1, 0);
        return true;
    }

    // Page up/down for scrolling
    if (kc == juce::KeyPress::pageUpKey)   { moveCursor (0, -visibleRows); return true; }
    if (kc == juce::KeyPress::pageDownKey) { moveCursor (0,  visibleRows); return true; }

    return false;
}

bool TrackerComponent::keyStateChanged (bool /*isKeyDown*/)
{
    return false;
}
