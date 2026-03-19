#pragma once
#include <JuceHeader.h>
#include "TrackerEngine.h"

//==============================================================================
// Shared LookAndFeel that forces Courier New on slider text boxes.
// Apply to sliders via setLookAndFeel() — child text-box Labels inherit it.
struct DialLAF : public juce::LookAndFeel_V4
{
    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font ("Courier New", 10.0f, juce::Font::plain);
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        // Draw the arc/outline using the default V4 implementation first
        LookAndFeel_V4::drawRotarySlider (g, x, y, width, height, sliderPos,
                                          rotaryStartAngle, rotaryEndAngle, slider);

        // Replicate JUCE V4's thumb position math exactly, then overdraw
        // the flat thumb circle with a sphere-like gradient.
        const auto bounds = juce::Rectangle<int> (x, y, width, height)
                                .toFloat().reduced (10.0f);
        const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const float lineW   = juce::jmin (8.0f, radius * 0.5f);
        const float arcR    = radius - lineW * 0.5f;
        const float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float thumbR  = lineW;   // half of thumbWidth = lineW*2

        const juce::Point<float> tp (
            bounds.getCentreX() + arcR * std::sin (toAngle),
            bounds.getCentreY() - arcR * std::cos (toAngle));

        // Sphere gradient: 10% closer to base yellow than previous
        juce::ColourGradient grad (
            juce::Colour (0xffFED546),          // subtle warm highlight (-5%)
            tp.x - thumbR * 0.45f, tp.y - thumbR * 0.55f,
            juce::Colour (0xffD39C0E),          // subtle amber shadow (-5%)
            tp.x + thumbR * 0.45f, tp.y + thumbR * 0.55f,
            false);                              // linear gradient
        g.setGradientFill (grad);
        g.fillEllipse (tp.x - thumbR, tp.y - thumbR, thumbR * 2.0f, thumbR * 2.0f);

        // Small specular dot — upper-left, soft white
        g.setColour (juce::Colours::white.withAlpha (0.19f));
        g.fillEllipse (tp.x - thumbR * 0.5f, tp.y - thumbR * 0.65f,
                       thumbR * 0.55f, thumbR * 0.42f);
    }
};

//==============================================================================
// Parameter panel shown at the bottom of the editor.
// Displays and edits the FMVoiceParams for the currently selected track.
class VoiceParamsPanel  : public juce::Component,
                          public juce::Slider::Listener
{
public:
    VoiceParamsPanel();
    ~VoiceParamsPanel() override;

    void paint  (juce::Graphics& g) override;
    void resized() override;

    // Call from editor whenever selected track changes
    void setTrack (TrackerTrack* track);

    void sliderValueChanged (juce::Slider* slider) override;

private:
    TrackerTrack* currentTrack { nullptr };

    void buildSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& name,
                      double min, double max, double defaultVal,
                      bool isInt = false, double midPoint = 0.0);

    void syncToTrack();   // push track params → sliders
    void pushToTrack();   // pull slider values → track params

    // LookAndFeel for Courier New text boxes (must outlive sliders)
    DialLAF dialLAF;

    // FM params sliders
    juce::Slider slModRatio, slModIndex;
    juce::Slider slCAtk, slCDcy, slCSus, slCRel;
    juce::Slider slMAtk, slMDcy, slMSus, slMRel;
    juce::Slider slBitDepth, slSrDiv;
    juce::Slider slFilterCut, slVolume;
    juce::ToggleButton btnFilterLP;

    juce::Label  lblModRatio, lblModIndex;
    juce::Label  lblCAtk, lblCDcy, lblCSus, lblCRel;
    juce::Label  lblMAtk, lblMDcy, lblMSus, lblMRel;
    juce::Label  lblBitDepth, lblSrDiv;
    juce::Label  lblFilterCut, lblVolume;

    juce::Label lblFMSection, lblCarrierSection, lblModSection, lblLoFiSection;

    // Bounding boxes for each dial (filled in resized, drawn in paint)
    juce::Array<juce::Rectangle<int>> dialBoxes;

    bool ignoreCallbacks { false };

    juce::Colour colBg     { 0xff1a1a18 };  // warm dark grey
    juce::Colour colText   { 0xffE8E3E4 };  // platinum
    juce::Colour colAccent { 0xffFDC618 };  // Mikado yellow

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceParamsPanel)
};
