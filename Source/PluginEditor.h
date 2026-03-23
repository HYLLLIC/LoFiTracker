#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TrackerComponent.h"
#include "VoiceParamsPanel.h"

//==============================================================================
// Square-cornered LookAndFeel for toolbar buttons and dialogs — flat retro style.
struct SquareButtonLAF : public juce::LookAndFeel_V4
{
    SquareButtonLAF()
    {
        // Defaults for AlertWindow dialog buttons (individual buttons
        // may override via setColour() which takes priority over LAF defaults).
        setColour (juce::TextButton::buttonColourId,         juce::Colour (0xff1e2a10));
        setColour (juce::TextButton::textColourOffId,        juce::Colour (0xff4C7030));

        // AlertWindow background / text
        setColour (juce::AlertWindow::backgroundColourId,    juce::Colour (0xff1d1d1b));
        setColour (juce::AlertWindow::textColourId,          juce::Colour (0xffE8E3E4));
        setColour (juce::AlertWindow::outlineColourId,       juce::Colour (0xff4C7030));
    }

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

        // Border: olive when toggled-on, otherwise auto-shade
        const juce::Colour border = button.getToggleState()
                                    ? juce::Colour (0xff4C7030)
                                    : fill.brighter (0.3f);
        g.setColour (border);
        g.drawRect (bounds, 1.0f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool isMouseOverButton,
                         bool isButtonDown) override
    {
        juce::Colour col = button.findColour (
            button.getToggleState() ? juce::TextButton::textColourOnId
                                    : juce::TextButton::textColourOffId);

        // Brighten text on hover/press for non-toggled state
        if (!button.getToggleState())
        {
            if (isButtonDown)      col = col.brighter (0.55f);
            else if (isMouseOverButton) col = col.brighter (0.4f);
        }

        g.setFont (juce::Font ("Courier New", 11.0f, juce::Font::plain));
        g.setColour (col);
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (2, 0),
                          juce::Justification::centred, 1);
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

    //-- look-and-feel (must outlive the buttons/sliders)
    SquareButtonLAF  squareButtonLAF;
    DialLAF          sliderLAF;       // Courier New for toolbar slider text boxes

    //-- toolbar widgets
    juce::TextButton btnPlay   { "PLAY" };
    juce::TextButton btnStop   { "STOP" };
    juce::Slider     slBpm;
    juce::Label      lblBpm;
    juce::Label      lblTitle;

    //-- master slide / stutter defaults (toolbar)
    juce::Label      lblMasterSlide;
    juce::Slider     slMasterSlideLen;              // 0–100 %
    juce::Label      lblMasterStutter;
    juce::Slider     slMasterStutterCount;          // 1–4

    //-- reset
    juce::TextButton btnReset          { "RESET" };

    //-- main areas
    TrackerComponent tracker;
    VoiceParamsPanel paramsPanel;

    //-- layout constants
    static constexpr int kToolbarH   = 36;
    static constexpr int kParamsPanelH = 110;
    static constexpr int kTotalW     = 800;
    static constexpr int kTotalH     = 570;  // 570-36-110-46 = 378 = 21*18, zero gap

    void setupToolbar();
    void updateParamsPanel();       // push selected track to panel
    void showResetConfirmation();   // show "Are you sure?" dialog
    void doReset();                 // execute the reset

    bool lastPlayState { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiTrackerAudioProcessorEditor)
};
