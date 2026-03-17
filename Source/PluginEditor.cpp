#include "PluginEditor.h"

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
    btnPlay.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e2a10));  // dark olive bg
    btnPlay.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff4C7030));  // olive text
    btnPlay.onClick = [this]
    {
        processor.setInternalPlaying (true);
        tracker.grabKeyboardFocus();
    };

    // Stop button
    addAndMakeVisible (btnStop);
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
    lblTitle.setBounds (tx, 6, 140, 24);  tx += 148;
    btnPlay.setBounds  (tx, 6, 52, 24);   tx += 56;
    btnStop.setBounds  (tx, 6, 52, 24);   tx += 60;
    lblBpm.setBounds   (tx, 8, 32, 20);   tx += 34;
    slBpm.setBounds    (tx, 5, 180, 26);

    // Main tracker grid
    const int trackerH = h - kToolbarH - kParamsPanelH;
    tracker.setBounds (0, kToolbarH, w, trackerH);

    // Voice params panel at bottom
    paramsPanel.setBounds (0, kToolbarH + trackerH, w, kParamsPanelH);
}
