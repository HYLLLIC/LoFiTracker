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

    // Master defaults applied to every new note entered via keyboard.
    // Individual per-step overrides (via the popup) take priority.
    void setMasterSlide        (bool s)   { masterSlide        = s; }
    void setMasterSlideLen     (float v)  { masterSlideLen     = juce::jlimit (0.0f, 1.0f, v); }
    void setMasterStutter      (bool s)   { masterStutter      = s; }
    void setMasterStutterCount (int n)    { masterStutterCount = juce::jlimit (1, 4, n); }

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

    // Master defaults for new note entry
    bool  masterSlide        { false };
    float masterSlideLen     { 0.5f  };
    bool  masterStutter      { false };
    int   masterStutterCount { 2     };
    juce::Rectangle<int> popupRect;  // set during drawNoteEditPopup, used for hit-testing

    void drawNoteEditPopup (juce::Graphics& g);
    void handlePopupClick  (int relX, int relY);   // click inside popup rect

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

    //-- colour scheme  (olive / platinum / Mikado yellow retro palette)
    juce::Colour colBg         { 0xff1a1a18 };  // warm dark grey
    juce::Colour colHeaderBg   { 0xff222220 };
    juce::Colour colRowEven    { 0xff1d1d1b };
    juce::Colour colRowOdd     { 0xff202020 };
    juce::Colour colRowGroup   { 0xff262620 };  // every 4th row (beat marker)
    juce::Colour colCursor     { 0xff2a3a18 };  // dark olive — selected cell
    juce::Colour colPlayhead   { 0xff3d5a24 };  // olive — current playing step
    juce::Colour colText       { 0xffE8E3E4 };  // platinum
    juce::Colour colDimText    { 0xff666655 };
    juce::Colour colAccent     { 0xffFDC618 };  // Mikado yellow
    juce::Colour colMuted      { 0xff8B5A20 };  // warm amber-brown for muted
    juce::Colour colDivider    { 0xff333330 };
    juce::Colour colStepNum    { 0xff4a4a38 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackerComponent)
};
