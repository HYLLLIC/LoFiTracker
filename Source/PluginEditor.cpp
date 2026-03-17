#include "PluginEditor.h"
#include "MacWindowHelper.h"


//==============================================================================
LoFiTrackerAudioProcessorEditor::LoFiTrackerAudioProcessorEditor (LoFiTrackerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      tracker   (p.getEngine())
{
    setSize (kTotalW, kTotalH);
    setResizable (true, false);
    setResizeLimits (500, 400, 1400, 900);
    setWantsKeyboardFocus (true);

    setupToolbar();

    addAndMakeVisible (tracker);
    addAndMakeVisible (paramsPanel);

    // When selected track changes in the grid, update the params panel
    tracker.onStepCountChanged = [this] (int, int) { repaint(); };

    tracker.onTogglePlay = [this]
    {
        const bool nowPlaying = !processor.getEngine().isPlaying();
        processor.setInternalPlaying (nowPlaying);
    };

    startTimerHz (20);

    updateParamsPanel();
}

LoFiTrackerAudioProcessorEditor::~LoFiTrackerAudioProcessorEditor()
{
    btnPlay.setLookAndFeel          (nullptr);
    btnStop.setLookAndFeel          (nullptr);
    btnMasterSlide.setLookAndFeel   (nullptr);
    btnMasterStutter.setLookAndFeel (nullptr);
    btnReset.setLookAndFeel         (nullptr);
    stopTimer();
}

//==============================================================================
void LoFiTrackerAudioProcessorEditor::setupToolbar()
{
    // Title
    addAndMakeVisible (lblTitle);
    lblTitle.setText ("LoFiTracker", juce::dontSendNotification);
    lblTitle.setFont (juce::Font ("Courier New", 16.0f, juce::Font::bold));
    lblTitle.setColour (juce::Label::textColourId, juce::Colour (0xffFDC618));  // Mikado yellow

    // Play button
    addAndMakeVisible (btnPlay);
    btnPlay.setLookAndFeel (&squareButtonLAF);
    btnPlay.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e2a10));  // dark olive bg
    btnPlay.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff4C7030));  // olive text
    btnPlay.onClick = [this]
    {
        processor.setInternalPlaying (true);
        tracker.grabKeyboardFocus();
    };

    // Stop button
    addAndMakeVisible (btnStop);
    btnStop.setLookAndFeel (&squareButtonLAF);
    btnStop.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2a1e0a));  // dark warm bg
    btnStop.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff8B5A20));  // amber-brown text
    btnStop.onClick = [this]
    {
        processor.setInternalPlaying (false);
        tracker.grabKeyboardFocus();
    };

    // BPM slider
    addAndMakeVisible (slBpm);
    slBpm.setSliderStyle (juce::Slider::LinearHorizontal);
    slBpm.setRange (20.0, 300.0, 0.5);
    slBpm.setValue (120.0, juce::dontSendNotification);
    slBpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
    slBpm.setColour (juce::Slider::trackColourId,       juce::Colour (0xff4C7030).withAlpha (0.6f));  // olive
    slBpm.setColour (juce::Slider::backgroundColourId,  juce::Colour (0xff1a1a18));
    slBpm.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffE8E3E4));  // platinum
    slBpm.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff141412));
    slBpm.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff333330));
    slBpm.onValueChange = [this]
    {
        processor.setInternalBpm (slBpm.getValue());
    };

    addAndMakeVisible (lblBpm);
    lblBpm.setText ("BPM", juce::dontSendNotification);
    lblBpm.setFont (juce::Font ("Courier New", 11.0f, juce::Font::plain));
    lblBpm.setColour (juce::Label::textColourId, juce::Colour (0xff666655));  // dim platinum

    // ---- Master slide toggle [s] ----
    addAndMakeVisible (btnMasterSlide);
    btnMasterSlide.setLookAndFeel (&squareButtonLAF);
    btnMasterSlide.setClickingTogglesState (true);
    btnMasterSlide.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff141a0c));
    btnMasterSlide.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2d4a18));
    btnMasterSlide.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff4C7030).withAlpha (0.5f));
    btnMasterSlide.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffFDC618));
    btnMasterSlide.onClick = [this]
    {
        tracker.setMasterSlide (btnMasterSlide.getToggleState());
    };

    // ---- Master slide length (0–100 %) ----
    addAndMakeVisible (slMasterSlideLen);
    slMasterSlideLen.setSliderStyle (juce::Slider::LinearHorizontal);
    slMasterSlideLen.setRange (0.0, 100.0, 1.0);
    slMasterSlideLen.setValue (50.0, juce::dontSendNotification);
    slMasterSlideLen.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 30, 20);
    slMasterSlideLen.setTextValueSuffix ("%");
    slMasterSlideLen.setColour (juce::Slider::trackColourId,              juce::Colour (0xff4C7030).withAlpha (0.5f));
    slMasterSlideLen.setColour (juce::Slider::backgroundColourId,         juce::Colour (0xff1a1a18));
    slMasterSlideLen.setColour (juce::Slider::textBoxTextColourId,        juce::Colour (0xffE8E3E4));
    slMasterSlideLen.setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colour (0xff141412));
    slMasterSlideLen.setColour (juce::Slider::textBoxOutlineColourId,     juce::Colour (0xff333330));
    slMasterSlideLen.onValueChange = [this]
    {
        tracker.setMasterSlideLen ((float) slMasterSlideLen.getValue() / 100.0f);
    };

    // ---- Master stutter toggle [//] ----
    addAndMakeVisible (btnMasterStutter);
    btnMasterStutter.setLookAndFeel (&squareButtonLAF);
    btnMasterStutter.setClickingTogglesState (true);
    btnMasterStutter.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff141a0c));
    btnMasterStutter.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2d4a18));
    btnMasterStutter.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff4C7030).withAlpha (0.5f));
    btnMasterStutter.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffFDC618));
    btnMasterStutter.onClick = [this]
    {
        tracker.setMasterStutter (btnMasterStutter.getToggleState());
    };

    // ---- Master stutter count (1–4) ----
    addAndMakeVisible (slMasterStutterCount);
    slMasterStutterCount.setSliderStyle (juce::Slider::LinearHorizontal);
    slMasterStutterCount.setRange (1.0, 4.0, 1.0);
    slMasterStutterCount.setValue (2.0, juce::dontSendNotification);
    slMasterStutterCount.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 20, 20);
    slMasterStutterCount.setColour (juce::Slider::trackColourId,             juce::Colour (0xff4C7030).withAlpha (0.5f));
    slMasterStutterCount.setColour (juce::Slider::backgroundColourId,        juce::Colour (0xff1a1a18));
    slMasterStutterCount.setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffE8E3E4));
    slMasterStutterCount.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff141412));
    slMasterStutterCount.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour (0xff333330));
    slMasterStutterCount.onValueChange = [this]
    {
        tracker.setMasterStutterCount (juce::roundToInt (slMasterStutterCount.getValue()));
    };

    // ---- Reset button ----
    addAndMakeVisible (btnReset);
    btnReset.setLookAndFeel (&squareButtonLAF);
    btnReset.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff221010));  // dark red tint
    btnReset.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff8B3020));  // dim red
    btnReset.onClick = [this] { showResetConfirmation(); };
}

//==============================================================================
void LoFiTrackerAudioProcessorEditor::showResetConfirmation()
{
    auto* aw = new juce::AlertWindow ("Reset",
                                      "Erase all notes and reset all voice settings?\n"
                                      "(BPM is preserved.)",
                                      juce::MessageBoxIconType::NoIcon,
                                      this);
    aw->setLookAndFeel (&squareButtonLAF);
    aw->addButton ("YES", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("NO",  0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (
        true,
        juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            aw->setLookAndFeel (nullptr);
            if (result == 1)
                doReset();
        }),
        true);  // deleteWhenDismissed
}

void LoFiTrackerAudioProcessorEditor::doReset()
{
    processor.resetAll();

    // Reset master control UI to defaults
    btnMasterSlide.setToggleState    (false, juce::dontSendNotification);
    btnMasterStutter.setToggleState  (false, juce::dontSendNotification);
    slMasterSlideLen.setValue        (50.0,  juce::dontSendNotification);
    slMasterStutterCount.setValue    (2.0,   juce::dontSendNotification);

    tracker.setMasterSlide        (false);
    tracker.setMasterSlideLen     (0.5f);
    tracker.setMasterStutter      (false);
    tracker.setMasterStutterCount (2);
}

//==============================================================================
bool LoFiTrackerAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Catch spacebar at the editor level so it works regardless of focus
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        processor.setInternalPlaying (!processor.getEngine().isPlaying());
        return true;
    }
    return false;
}

//==============================================================================
void LoFiTrackerAudioProcessorEditor::timerCallback()
{
    // Sync BPM slider from engine (in case host changed it)
    const double engineBpm = processor.getEngine().getBpm();
    if (std::abs (slBpm.getValue() - engineBpm) > 0.1)
        slBpm.setValue (engineBpm, juce::dontSendNotification);

    // Update params panel when selected track changes
    updateParamsPanel();
}

void LoFiTrackerAudioProcessorEditor::updateParamsPanel()
{
    const int sel = tracker.getSelectedTrack();
    paramsPanel.setTrack (&processor.getEngine().getTrack (sel));
}

//==============================================================================
void LoFiTrackerAudioProcessorEditor::parentHierarchyChanged()
{
    // Only present in standalone — no DocumentWindow inside Ableton/VST3
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
    {
       #if JUCE_MAC
        // Force Aqua (light) appearance: native traffic-light buttons,
        // white title bar, black text — regardless of system dark/light mode.
        if (auto* peer = dw->getPeer())
            applyLightTitleBar (peer);
       #endif
    }
}

//==============================================================================
void LoFiTrackerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a18));  // warm dark grey

    // Toolbar background
    g.setColour (juce::Colour (0xff222220));
    g.fillRect (0, 0, getWidth(), kToolbarH);

    // Toolbar bottom line — olive accent
    g.setColour (juce::Colour (0xff4C7030).withAlpha (0.5f));
    g.drawHorizontalLine (kToolbarH - 1, 0.0f, (float) getWidth());
}

void LoFiTrackerAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Toolbar row
    int tx = 8;
    lblTitle.setBounds          (tx, 6, 140, 24);  tx += 148;
    btnPlay.setBounds           (tx, 6, 52, 24);   tx += 56;
    btnStop.setBounds           (tx, 6, 52, 24);   tx += 60;
    lblBpm.setBounds            (tx, 8, 32, 20);   tx += 34;
    slBpm.setBounds             (tx, 5, 120, 26);  tx += 126;

    // ---- Master slide ----
    btnMasterSlide.setBounds    (tx, 6, 22, 24);   tx += 26;
    slMasterSlideLen.setBounds  (tx, 5, 68, 26);   tx += 72;

    // ---- Master stutter ----
    btnMasterStutter.setBounds      (tx, 6, 26, 24);   tx += 30;
    slMasterStutterCount.setBounds  (tx, 5, 48, 26);   tx += 52;

    // ---- Reset — right-aligned with 8px margin ----
    btnReset.setBounds (w - 60, 6, 52, 24);

    // Main tracker grid
    const int trackerH = h - kToolbarH - kParamsPanelH;
    tracker.setBounds (0, kToolbarH, w, trackerH);

    // Voice params panel at bottom
    paramsPanel.setBounds (0, kToolbarH + trackerH, w, kParamsPanelH);
}
