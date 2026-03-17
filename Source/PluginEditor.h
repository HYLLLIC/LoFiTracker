#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TrackerComponent.h"
#include "VoiceParamsPanel.h"

//==============================================================================
// Square-cornered LookAndFeel for toolbar buttons — flat retro style.
struct SquareButtonLAF : public juce::LookAndFeel_V4
{
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool isMouseOverButton,
                               bool isButtonDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat();

        juce::Colour fill = backgroundColour;
        if (isButtonDown)
            fill = fill.brighter (0.15f);
        else if (isMouseOverButton)
            fill = fill.brighter (0.07f);

        g.setColour (fill);
        g.fillRect (bounds);

        // Subtle border — one shade lighter than the fill
        g.setColour (fill.brighter (0.3f));
        g.drawRect (bounds, 1.0f);
    }
};

//==============================================================================
class LoFiTrackerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                          public juce::Timer
{
public:
    LoFiTrackerAudioProcessorEditor (LoFiTrackerAudioProcessor& p);
    ~LoFiTrackerAudioProcessorEditor() override;

    void paint                  (juce::Graphics& g) override;
    void resized                () override;
    bool keyPressed             (const juce::KeyPress& key) override;
    void parentHierarchyChanged () override;  // styles the standalone window frame

    void timerCallback() override;  // sync UI ↔ engine state

private:
    LoFiTrackerAudioProcessor& processor;

    //-- look-and-feel (must outlive the buttons)
    SquareButtonLAF  squareButtonLAF;

    //-- toolbar widgets
    juce::TextButton btnPlay   { "PLAY" };
    juce::TextButton btnStop   { "STOP" };
    juce::Slider     slBpm;
    juce::Label      lblBpm;
    juce::Label      lblTitle;

    //-- main areas
    TrackerComponent tracker;
    VoiceParamsPanel paramsPanel;

    //-- layout constants
    static constexpr int kToolbarH   = 36;
    static constexpr int kParamsPanelH = 110;
    static constexpr int kTotalW     = 800;
    static constexpr int kTotalH     = 580;

    void setupToolbar();
    void updateParamsPanel();   // push selected track to panel

    bool lastPlayState { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiTrackerAudioProcessorEditor)
};
