#pragma once
#include <JuceHeader.h>
#include "TrackerEngine.h"

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

    bool ignoreCallbacks { false };

    juce::Colour colBg     { 0xff1e1e1e };
    juce::Colour colText   { 0xffaaaaaa };
    juce::Colour colAccent { 0xff44cc66 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceParamsPanel)
};
