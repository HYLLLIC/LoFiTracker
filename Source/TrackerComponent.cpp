#include "TrackerComponent.h"
#include "BinaryData.h"

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

    // Load background artwork from embedded binary data
    bgImage = juce::ImageCache::getFromMemory (BinaryData::bg_jpg, BinaryData::bg_jpgSize);
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

    // Draw artwork as a very low-opacity background behind the cell area only.
    // Crop 4% off each edge of the source image to remove borders / outer detail.
    if (bgImage.isValid())
    {
        const auto destRect = getLocalBounds().withTrimmedTop (headerH).toFloat();
        const float crop = 0.04f;
        const juce::Rectangle<float> srcRect (
            bgImage.getWidth()  * crop,
            bgImage.getHeight() * crop,
            bgImage.getWidth()  * (1.0f - 2.0f * crop),
            bgImage.getHeight() * (1.0f - 2.0f * crop));

        g.setOpacity (0.20f);
        g.drawImage (bgImage,
                     destRect.getX(), destRect.getY(), destRect.getWidth(), destRect.getHeight(),
                     (int) srcRect.getX(), (int) srcRect.getY(),
                     (int) srcRect.getWidth(), (int) srcRect.getHeight());
        g.setOpacity (1.0f);
    }

    const int totalW = getWidth();

    // Snapshot all display steps once before drawing so every column
    // reflects the same logical step boundary (no inter-column stagger)
    int playStep[kNumTracks];
    for (int t = 0; t < kNumTracks; ++t)
        playStep[t] = engine.getDisplayStep (t);

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
            g.drawText (track.name, x + 4, 2, colW - 28, 16, juce::Justification::centredLeft, false);

            // Mute button — top-right corner, same row as voice name
            {
                const juce::Rectangle<int> muteBtn (x + colW - 22, 2, 18, 16);
                const juce::Colour muteActive (0xff8B3020);   // dim red, matches Reset
                const juce::Colour muteBg     (0xff221010);   // dark red tint, matches Reset
                g.setColour (track.muted ? muteBg : juce::Colour (0x00000000));
                g.fillRect (muteBtn);
                g.setColour (track.muted ? muteActive : colDimText.withAlpha (0.35f));
                g.drawRect (muteBtn, 1);
                g.setFont (juce::Font ("Courier New", 11.0f, juce::Font::bold));
                g.setColour (track.muted ? muteActive : colDimText.withAlpha (0.35f));
                g.drawText ("M", muteBtn, juce::Justification::centred, false);
            }

            // Step count controls: "LEN [<] 16 [>]"
            const int btnW = 14, btnH = 13;
            const int midY = 20;
            const juce::String lenStr = juce::String (track.stepCount);

            g.setFont (juce::Font ("Courier New", 11.0f, juce::Font::plain));
            g.setColour (colText);
            g.drawText ("LEN:", x + 2, midY, 30, btnH, juce::Justification::left, false);

            // Decrease / increase buttons as proper bordered rectangles
            const juce::Rectangle<int> decBtn (x + 33, midY, btnW, btnH);
            const juce::Rectangle<int> incBtn (x + 68, midY, btnW, btnH);
            g.setColour (colText.withAlpha (0.08f));
            g.fillRect (decBtn);
            g.fillRect (incBtn);
            g.setColour (colDimText.withAlpha (0.55f));
            g.drawRect (decBtn, 1);
            g.drawRect (incBtn, 1);
            g.setColour (colText);
            g.drawText ("<", decBtn, juce::Justification::centred, false);
            g.drawText (">", incBtn, juce::Justification::centred, false);

            // Step count number between buttons
            g.drawText (lenStr, x + 48, midY, 20, btnH, juce::Justification::centred, false);

            g.setColour (colDimText);
        }

        // ---- Rows (steps) ----
        const int maxRow = juce::jmax (track.stepCount, 1);

        for (int row = scrollOffset; row < scrollOffset + visibleRows; ++row)
        {
            const juce::Rectangle<int> cell = cellRect (t, row);
            if (cell.isEmpty()) continue;

            const bool inPattern = (row < maxRow);
            const bool isPlay    = inPattern && (row == playStep[t]) && engine.isPlaying();
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

            // Reserve 32px on the right for slide/stutter indicators
            g.drawText (cellText, cell.getX() + 22, cell.getY(), cell.getWidth() - 56, cell.getHeight(),
                        juce::Justification::centredLeft, false);

            // --- Slide "s" and stutter "//" indicators ---
            g.setFont (juce::Font ("Courier New", 11.0f, juce::Font::bold));

            // Slide  — rightmost 32px: x + colW-32
            const int flagBaseX = cell.getRight() - 32;
            g.setColour (step.slide
                         ? colAccent
                         : colDimText.withAlpha (0.35f));
            g.drawText ("s", flagBaseX, cell.getY(), 12, cell.getHeight(),
                        juce::Justification::centredLeft, false);

            // Stutter — further right: x + colW-18
            g.setColour (step.stutter
                         ? colAccent
                         : colDimText.withAlpha (0.35f));
            g.drawText ("//", flagBaseX + 14, cell.getY(), 18, cell.getHeight(),
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

    if (noteEditMode)
        drawNoteEditPopup (g);
}

//==============================================================================
void TrackerComponent::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    const int clickX = e.x;
    const int clickY = e.y;

    // If the note-edit popup is open, check if the click is inside it first.
    if (noteEditMode && !popupRect.isEmpty() && popupRect.contains (clickX, clickY))
    {
        handlePopupClick (clickX - popupRect.getX(), clickY - popupRect.getY());
        repaint();
        return;
    }

    for (int t = 0; t < kNumTracks; ++t)
    {
        const int x = t * colW;
        if (clickX < x || clickX >= x + colW) continue;

        selectedTrack = t;

        if (clickY < headerH)
        {
            noteEditMode = false;
            handleHeaderClick (t, { clickX - x, clickY });
        }
        else
        {
            const int row = (clickY - headerH) / rowH + scrollOffset;
            if (row >= 0 && row < kMaxSteps)
            {
                const int  localX    = clickX - x;
                const bool inPattern = (row < engine.getTrack (t).stepCount);

                // Check slide/stutter indicator click zones (right-side flags).
                // These toggle on click without opening the note-edit popup.
                if (inPattern)
                {
                    auto& flagStep = engine.getTrack (t).steps[row];

                    if (localX >= colW - 18)           // stutter "//" zone
                    {
                        selectedStep = row;
                        noteEditMode = false;
                        flagStep.stutter = !flagStep.stutter;
                        repaint();
                        break;
                    }
                    if (localX >= colW - 32)           // slide "s" zone
                    {
                        selectedStep = row;
                        noteEditMode = false;
                        flagStep.slide = !flagStep.slide;
                        repaint();
                        break;
                    }
                }

                selectedStep = row;
                const auto& step = engine.getTrack (t).steps[row];
                noteEditMode = (step.note > 0);
            }
        }
        repaint();
        break;
    }
}

void TrackerComponent::handleHeaderClick (int trackIdx, const juce::Point<int>& local)
{
    auto& track = engine.getTrack (trackIdx);

    // Mute button: top-right corner, y 2..18, x colW-22..colW-4
    if (local.y >= 2 && local.y <= 18 && local.x >= colW - 22 && local.x <= colW - 4)
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
void TrackerComponent::mouseWheelMove (const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& wheel)
{
    if (!noteEditMode) return;

    auto& step = engine.getTrack (selectedTrack).steps[selectedStep];
    if (step.note <= 0) return;

    const int dir = (wheel.deltaY > 0.0f) ? 1 : -1;

    // If the mouse is over the length bar area of the popup (row 0, right side),
    // scroll the relevant length control; otherwise scroll velocity.
    if (!popupRect.isEmpty()
        && e.y >= popupRect.getY() + 5  && e.y < popupRect.getY() + 22
        && e.x >= popupRect.getX() + 128)
    {
        if (step.stutter)
            step.stutterCount = juce::jlimit (1, 4, step.stutterCount + dir);
        else if (step.slide)
            step.slideLen = juce::jlimit (0.0f, 1.0f, step.slideLen + dir * 0.1f);
    }
    else if (!popupRect.isEmpty()
             && e.y >= popupRect.getY() + 22 && e.y < popupRect.getY() + 44)
    {
        // Row 1 hover (button row) — no scroll action
    }
    else
    {
        // Velocity: each tick = ±4; row 2/3 of popup or anywhere else
        step.vel = (uint8_t) juce::jlimit (1, 127, (int) step.vel + dir * 4);
    }

    repaint();
}

//==============================================================================
void TrackerComponent::drawNoteEditPopup (juce::Graphics& g)
{
    const auto& step = engine.getTrack (selectedTrack).steps[selectedStep];
    if (step.note <= 0) { noteEditMode = false; return; }

    // Popup dimensions — expanded to fit slide/stutter controls
    constexpr int pw = 210, ph = 84;

    // Anchor near the selected cell, clamped to component bounds
    const juce::Rectangle<int> anchor = cellRect (selectedTrack, selectedStep);
    int px = anchor.getRight() + 4;
    int py = anchor.getY() - 4;
    if (px + pw > getWidth())  px = anchor.getX() - pw - 4;
    if (py + ph > getHeight()) py = getHeight() - ph - 4;
    py = juce::jmax (headerH + 2, py);

    popupRect = juce::Rectangle<int> (px, py, pw, ph);

    // Background
    g.setColour (juce::Colour (0xee1e1e18));
    g.fillRoundedRectangle (popupRect.toFloat(), 4.0f);
    g.setColour (juce::Colour (0xff4C7030).withAlpha (0.9f));  // olive border
    g.drawRoundedRectangle (popupRect.toFloat(), 4.0f, 1.0f);

    const juce::Font mono12 ("Courier New", 12.0f, juce::Font::bold);
    const juce::Font mono10 ("Courier New", 10.0f, juce::Font::plain);
    const juce::Font mono11 ("Courier New", 11.0f, juce::Font::bold);

    // ---- Row 0: note name + navigation hints ----
    g.setFont (mono12);
    g.setColour (colAccent);
    g.drawText (noteToString (step.note),
                px + 6, py + 5, 44, 16,
                juce::Justification::centredLeft, false);

    g.setFont (mono10);
    g.setColour (colDimText);
    g.drawText ("<> semi  ^v oct",
                px + 52, py + 7, 72, 12,
                juce::Justification::centredLeft, false);

    // ---- Row 0, top-right: length slider ----
    // Determines stutter count (1-4) or slide length (0-1) depending on active flag.
    {
        const int lblX  = px + 128;
        const int barX  = lblX + 22;
        const int barY  = py + 8;
        const int barW  = 44;
        const int barH  = 10;

        g.setFont (mono10);
        g.setColour (colDimText);
        g.drawText ("Len", lblX, barY, 20, barH, juce::Justification::centredLeft, false);

        // Trough
        g.setColour (juce::Colour (0xff282822));
        g.fillRoundedRectangle ((float)barX, (float)barY, (float)barW, (float)barH, 2.0f);

        if (step.stutter)
        {
            // Segmented bar: 4 equal blocks
            const int segW = barW / 4;
            for (int i = 0; i < 4; ++i)
            {
                const bool filled = (i < step.stutterCount);
                g.setColour (filled
                             ? juce::Colour (0xff4C7030).withAlpha (0.85f)
                             : juce::Colour (0xff333330));
                g.fillRect (barX + i * segW + (i > 0 ? 1 : 0), barY,
                            segW - (i > 0 ? 1 : 0), barH);
            }
            // Value label
            g.setFont (mono10);
            g.setColour (colText);
            g.drawText (juce::String (step.stutterCount),
                        barX + barW + 3, barY, 12, barH, juce::Justification::centredLeft, false);
        }
        else if (step.slide)
        {
            // Continuous fill
            g.setColour (juce::Colour (0xff4C7030).withAlpha (0.85f));
            g.fillRoundedRectangle ((float)barX, (float)barY,
                                    barW * step.slideLen, (float)barH, 2.0f);
            // Value as percentage
            g.setFont (mono10);
            g.setColour (colText);
            g.drawText (juce::String (juce::roundToInt (step.slideLen * 100)) + "%",
                        barX + barW + 3, barY, 20, barH, juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour (colDimText.withAlpha (0.3f));
            g.fillRoundedRectangle ((float)barX, (float)barY, (float)barW, (float)barH, 2.0f);
        }
    }

    // ---- Row 1: slide [s] and stutter [//] toggle buttons ----
    {
        const int btnY = py + 24;
        const int btnH = 16;

        // Slide button
        const juce::Rectangle<int> slideBtn (px + 6, btnY, 28, btnH);
        g.setColour (step.slide
                     ? juce::Colour (0xff4C7030).withAlpha (0.7f)
                     : juce::Colour (0xff282822));
        g.fillRect (slideBtn);
        g.setColour (step.slide ? colAccent : colDimText);
        g.drawRect (slideBtn, 1);
        g.setFont (mono11);
        g.setColour (step.slide ? colAccent : colDimText);
        g.drawText ("s", slideBtn, juce::Justification::centred, false);

        // Stutter button
        const juce::Rectangle<int> stutterBtn (px + 40, btnY, 34, btnH);
        g.setColour (step.stutter
                     ? juce::Colour (0xff4C7030).withAlpha (0.7f)
                     : juce::Colour (0xff282822));
        g.fillRect (stutterBtn);
        g.setColour (step.stutter ? colAccent : colDimText);
        g.drawRect (stutterBtn, 1);
        g.setFont (mono11);
        g.setColour (step.stutter ? colAccent : colDimText);
        g.drawText ("//", stutterBtn, juce::Justification::centred, false);

        // Hint
        g.setFont (mono10);
        g.setColour (colDimText.withAlpha (0.7f));
        g.drawText ("^v scroll to adjust",
                    px + 80, btnY, pw - 86, btnH, juce::Justification::centredLeft, false);
    }

    // ---- Row 2: velocity bar ----
    {
        const float velFrac = step.vel / 127.0f;
        const int barX = px + 6;
        const int barY = py + 46;
        const int barW = pw - 12;
        const int barH = 10;

        g.setColour (juce::Colour (0xff282822));
        g.fillRoundedRectangle ((float)barX, (float)barY, (float)barW, (float)barH, 3.0f);
        g.setColour (juce::Colour (0xff4C7030).withAlpha (0.85f));
        g.fillRoundedRectangle ((float)barX, (float)barY,
                                 barW * velFrac, (float)barH, 3.0f);
    }

    // ---- Row 3: velocity label ----
    g.setFont (mono10);
    g.setColour (colText);
    g.drawText ("vel " + velToString (step.vel) + "  ^v scroll",
                px + 6, py + 60, pw - 12, 11,
                juce::Justification::centredLeft, false);
}

//==============================================================================
void TrackerComponent::handlePopupClick (int relX, int relY)
{
    auto& step = engine.getTrack (selectedTrack).steps[selectedStep];
    if (step.note <= 0) return;

    constexpr int pw = 210;

    // Slide toggle button: row 1, x=6..34, y=24..40
    if (relY >= 24 && relY < 40 && relX >= 6 && relX < 34)
    {
        step.slide = !step.slide;
        return;
    }

    // Stutter toggle button: row 1, x=40..74, y=24..40
    if (relY >= 24 && relY < 40 && relX >= 40 && relX < 74)
    {
        step.stutter = !step.stutter;
        return;
    }

    // Length bar click (top-right: x=150..194, y=6..20) — set value from click position
    const int lenBarX = 150, lenBarW = 44;
    if (relY >= 6 && relY < 20 && relX >= lenBarX && relX < lenBarX + lenBarW)
    {
        const float frac = juce::jlimit (0.0f, 1.0f,
                                         (relX - lenBarX) / (float) lenBarW);
        if (step.stutter)
            step.stutterCount = juce::jlimit (1, 4, 1 + juce::roundToInt (frac * 3.0f));
        else if (step.slide)
            step.slideLen = frac;
    }

    juce::ignoreUnused (pw);
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
    step.note         = (int8_t) midiNote;
    step.vel          = 100;
    // Apply master defaults (individual popup overrides are per-step)
    step.slide        = masterSlide;
    step.slideLen     = masterSlideLen;
    step.stutter      = masterStutter;
    step.stutterCount = masterStutterCount;

    // Advance cursor down
    const int maxStep = engine.getTrack (selectedTrack).stepCount;
    selectedStep = (selectedStep + 1) % juce::jmax (1, maxStep);
    ensureCursorVisible();
}

void TrackerComponent::clearStep()
{
    auto& step = engine.getTrack (selectedTrack).steps[selectedStep];
    step.note         = 0;
    step.vel          = 0;
    step.slide        = false;
    step.slideLen     = 0.5f;
    step.stutter      = false;
    step.stutterCount = 2;
    noteEditMode = false;
    selectedStep = (selectedStep + 1) % juce::jmax (1, engine.getTrack (selectedTrack).stepCount);
    ensureCursorVisible();
}

void TrackerComponent::moveCursor (int dTrack, int dStep)
{
    noteEditMode  = false;
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

    // Note-edit popup mode: arrows edit the note, Escape exits
    if (noteEditMode)
    {
        auto& step = engine.getTrack (selectedTrack).steps[selectedStep];
        if (step.note > 0)
        {
            if (kc == juce::KeyPress::leftKey)
            {
                step.note = (int8_t) juce::jlimit (1, 127, (int) step.note - 1);
                repaint(); return true;
            }
            if (kc == juce::KeyPress::rightKey)
            {
                step.note = (int8_t) juce::jlimit (1, 127, (int) step.note + 1);
                repaint(); return true;
            }
            if (kc == juce::KeyPress::downKey)
            {
                step.note = (int8_t) juce::jlimit (1, 127, (int) step.note - 12);
                repaint(); return true;
            }
            if (kc == juce::KeyPress::upKey)
            {
                step.note = (int8_t) juce::jlimit (1, 127, (int) step.note + 12);
                repaint(); return true;
            }
        }
        if (kc == juce::KeyPress::escapeKey) { noteEditMode = false; repaint(); return true; }
    }

    // Navigation (only when not in note-edit mode)
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

    // Spacebar: toggle play/stop
    if (kc == juce::KeyPress::spaceKey)
    {
        if (onTogglePlay) onTogglePlay();
        return true;
    }

    return false;
}

bool TrackerComponent::keyStateChanged (bool /*isKeyDown*/)
{
    return false;
}
