#pragma once
#include <JuceHeader.h>
#include "TrackerEngine.h"

//==============================================================================
// Classic tracker grid component.
// 5 columns (one per track), each column scrolls its own steps independently.
// Keyboard-driven note entry: Z=C  S=C#  X=D  D=D#  C=E  V=F  G=F#  B=G
//                             H=G# N=A   J=A# M=B   number keys = octave
class TrackerComponent  : public juce::Component,
                          public juce::Timer
{
public:
    TrackerComponent (TrackerEngine& engine);
    ~TrackerComponent() override;

    //==========================================================================
    void paint    (juce::Graphics& g) override;
    void resized  () override;

    bool keyPressed      (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown       (const juce::MouseEvent& e) override;
    void mouseWheelMove  (const juce::MouseEvent& e,
                          const juce::MouseWheelDetails& wheel) override;

    void timerCallback() override;   // repaint playhead

    //==========================================================================
    int getSelectedTrack() const { return selectedTrack; }
    int getSelectedStep()  const { return selectedStep;  }

    // Callbacks set by editor
    std::function<void(int trackIdx, int newCount)> onStepCountChanged;
    std::function<void()> onTogglePlay;   // spacebar

private:
    TrackerEngine& engine;

    //-- cursor state
    int  selectedTrack { 0 };
    int  selectedStep  { 0 };
    int  currentOctave { 4 };
    bool noteEditMode  { false };  // popup note-edit overlay active

    void drawNoteEditPopup (juce::Graphics& g);

    //-- layout geometry (computed in resized())
    int headerH      { 46 };   // track header height
    int rowH         { 18 };   // step row height
    int colW         { 0 };    // computed per column
    int visibleRows  { 0 };    // number of rows that fit
    int scrollOffset { 0 };    // first visible row

    //-- display helpers
    juce::String noteToString  (int8_t note) const;
    juce::String velToString   (uint8_t vel) const;
    juce::Rectangle<int> cellRect (int track, int row) const;
    void scrollToCursor();
    void ensureCursorVisible();

    //-- note entry
    int  keyToNote (int keyCode) const;   // returns semitone 0..11, -1 = not a note key
    void enterNote (int semitone);
    void clearStep();
    void moveCursor (int dTrack, int dStep);

    //-- step-count spinners (simple click zones in header)
    void handleHeaderClick (int trackIdx, const juce::Point<int>& local);

    //-- colour scheme
    juce::Colour colBg         { 0xff1a1a1a };
    juce::Colour colHeaderBg   { 0xff222222 };
    juce::Colour colRowEven    { 0xff1e1e1e };
    juce::Colour colRowOdd     { 0xff212121 };
    juce::Colour colRowGroup   { 0xff252525 };  // every 4th row
    juce::Colour colCursor     { 0xff2a5c3a };  // selected cell
    juce::Colour colPlayhead   { 0xff3a6f4a };  // current playing step
    juce::Colour colText       { 0xffcccccc };
    juce::Colour colDimText    { 0xff555555 };
    juce::Colour colAccent     { 0xff44cc66 };
    juce::Colour colMuted      { 0xff884444 };
    juce::Colour colDivider    { 0xff333333 };
    juce::Colour colStepNum    { 0xff444444 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackerComponent)
};
