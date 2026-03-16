#include "VoiceParamsPanel.h"

//==============================================================================
VoiceParamsPanel::VoiceParamsPanel()
{
    auto addLbl = [&] (juce::Label& l, const juce::String& text)
    {
        addAndMakeVisible (l);
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Courier New", 10.0f, juce::Font::plain));
        l.setColour (juce::Label::textColourId, colText);
        l.setJustificationType (juce::Justification::centred);
    };

    auto addSl = [&] (juce::Slider& s, juce::Label& l, const juce::String& name,
                      double lo, double hi, double def, bool isInt = false)
    {
        buildSlider (s, l, name, lo, hi, def, isInt);
    };

    // Section labels
    addLbl (lblCarrierSection, "--- CARRIER ---");
    lblCarrierSection.setColour (juce::Label::textColourId, colAccent);
    addLbl (lblModSection, "--- MODULATOR ---");
    lblModSection.setColour (juce::Label::textColourId, colAccent);
    addLbl (lblLoFiSection, "--- LO-FI ---");
    lblLoFiSection.setColour (juce::Label::textColourId, colAccent);

    // FM
    addSl (slModRatio, lblModRatio, "Ratio",  0.125, 16.0, 2.0);
    addSl (slModIndex, lblModIndex, "Index",  0.0,   20.0, 5.0);

    // Carrier envelope
    addSl (slCAtk, lblCAtk, "C.Atk", 0.001, 2.0,  0.001);
    addSl (slCDcy, lblCDcy, "C.Dcy", 0.001, 4.0,  0.25);
    addSl (slCSus, lblCSus, "C.Sus", 0.0,   1.0,  0.0);
    addSl (slCRel, lblCRel, "C.Rel", 0.001, 2.0,  0.05);

    // Modulator envelope
    addSl (slMAtk, lblMAtk, "M.Atk", 0.001, 2.0,  0.001);
    addSl (slMDcy, lblMDcy, "M.Dcy", 0.001, 4.0,  0.12);
    addSl (slMSus, lblMSus, "M.Sus", 0.0,   1.0,  0.0);
    addSl (slMRel, lblMRel, "M.Rel", 0.001, 2.0,  0.05);

    // Lo-fi
    addSl (slBitDepth, lblBitDepth, "Bits",   1.0, 16.0, 8.0, true);
    addSl (slSrDiv,    lblSrDiv,    "SR÷",    1.0, 8.0,  1.0, true);
    addSl (slFilterCut,lblFilterCut,"Filter", 0.0, 1.0,  0.5);
    addSl (slVolume,   lblVolume,   "Volume", 0.0, 1.0,  0.8);

    // Filter type toggle
    addAndMakeVisible (btnFilterLP);
    btnFilterLP.setButtonText ("LP");
    btnFilterLP.setToggleState (true, juce::dontSendNotification);
    btnFilterLP.setColour (juce::ToggleButton::textColourId, colText);
    btnFilterLP.onClick = [this] { if (currentTrack) currentTrack->params.filterIsLP = btnFilterLP.getToggleState(); };
}

VoiceParamsPanel::~VoiceParamsPanel() {}

//==============================================================================
void VoiceParamsPanel::buildSlider (juce::Slider& s, juce::Label& l,
                                     const juce::String& name,
                                     double lo, double hi, double def, bool isInt)
{
    addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::LinearBarVertical);
    s.setRange (lo, hi, isInt ? 1.0 : 0.0);
    s.setValue (def, juce::dontSendNotification);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 14);
    s.setColour (juce::Slider::trackColourId,         juce::Colour (0xff44cc66).withAlpha (0.6f));
    s.setColour (juce::Slider::backgroundColourId,    juce::Colour (0xff2a2a2a));
    s.setColour (juce::Slider::textBoxTextColourId,   colText);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff161616));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff333333));
    s.addListener (this);

    addAndMakeVisible (l);
    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::Font ("Courier New", 9.0f, juce::Font::plain));
    l.setColour (juce::Label::textColourId, colText);
    l.setJustificationType (juce::Justification::centred);
}

//==============================================================================
void VoiceParamsPanel::setTrack (TrackerTrack* track)
{
    currentTrack = track;
    syncToTrack();
}

void VoiceParamsPanel::syncToTrack()
{
    if (currentTrack == nullptr) return;
    ignoreCallbacks = true;

    const auto& p = currentTrack->params;
    slModRatio.setValue  (p.modRatio,     juce::dontSendNotification);
    slModIndex.setValue  (p.modIndex,     juce::dontSendNotification);
    slCAtk.setValue      (p.cAttack,      juce::dontSendNotification);
    slCDcy.setValue      (p.cDecay,       juce::dontSendNotification);
    slCSus.setValue      (p.cSustain,     juce::dontSendNotification);
    slCRel.setValue      (p.cRelease,     juce::dontSendNotification);
    slMAtk.setValue      (p.mAttack,      juce::dontSendNotification);
    slMDcy.setValue      (p.mDecay,       juce::dontSendNotification);
    slMSus.setValue      (p.mSustain,     juce::dontSendNotification);
    slMRel.setValue      (p.mRelease,     juce::dontSendNotification);
    slBitDepth.setValue  (p.bitDepth,     juce::dontSendNotification);
    slSrDiv.setValue     (p.srDivisor,    juce::dontSendNotification);
    slFilterCut.setValue (p.filterCutoff, juce::dontSendNotification);
    slVolume.setValue    (p.volume,       juce::dontSendNotification);
    btnFilterLP.setToggleState (p.filterIsLP, juce::dontSendNotification);

    ignoreCallbacks = false;
}

void VoiceParamsPanel::pushToTrack()
{
    if (currentTrack == nullptr) return;
    auto& p = currentTrack->params;
    p.modRatio     = (float) slModRatio.getValue();
    p.modIndex     = (float) slModIndex.getValue();
    p.cAttack      = (float) slCAtk.getValue();
    p.cDecay       = (float) slCDcy.getValue();
    p.cSustain     = (float) slCSus.getValue();
    p.cRelease     = (float) slCRel.getValue();
    p.mAttack      = (float) slMAtk.getValue();
    p.mDecay       = (float) slMDcy.getValue();
    p.mSustain     = (float) slMSus.getValue();
    p.mRelease     = (float) slMRel.getValue();
    p.bitDepth     = (float) slBitDepth.getValue();
    p.srDivisor    = (int)   slSrDiv.getValue();
    p.filterCutoff = (float) slFilterCut.getValue();
    p.volume       = (float) slVolume.getValue();
    p.filterIsLP   = btnFilterLP.getToggleState();
}

void VoiceParamsPanel::sliderValueChanged (juce::Slider*)
{
    if (!ignoreCallbacks)
        pushToTrack();
}

//==============================================================================
void VoiceParamsPanel::paint (juce::Graphics& g)
{
    g.fillAll (colBg);
    g.setColour (juce::Colour (0xff333333));
    g.drawRect (getLocalBounds(), 1);
}

void VoiceParamsPanel::resized()
{
    // Layout: section labels + 14 sliders across the width
    const int w          = getWidth();
    const int h          = getHeight();
    const int lblH       = 14;
    const int totalSliders = 14;
    const int slW        = juce::jmax (24, (w - 4) / totalSliders);

    // Section header labels
    const int secLblH = 12;
    lblCarrierSection.setBounds (2,     2, 100, secLblH);
    lblModSection.setBounds     (104,   2, 120, secLblH);
    lblLoFiSection.setBounds    (226,   2, 100, secLblH);

    int x = 2;
    const int y = secLblH + 2;
    const int sH = h - y - lblH - 2;

    auto place = [&] (juce::Slider& s, juce::Label& l)
    {
        l.setBounds  (x, h - lblH, slW, lblH);
        s.setBounds  (x, y,        slW, sH);
        x += slW + 2;
    };

    // FM
    place (slModRatio, lblModRatio);
    place (slModIndex, lblModIndex);
    x += 4;  // gap between sections

    // Carrier env
    place (slCAtk, lblCAtk);
    place (slCDcy, lblCDcy);
    place (slCSus, lblCSus);
    place (slCRel, lblCRel);
    x += 4;

    // Mod env
    place (slMAtk, lblMAtk);
    place (slMDcy, lblMDcy);
    place (slMSus, lblMSus);
    place (slMRel, lblMRel);
    x += 4;

    // Lo-fi
    place (slBitDepth,  lblBitDepth);
    place (slSrDiv,     lblSrDiv);
    place (slFilterCut, lblFilterCut);
    place (slVolume,    lblVolume);

    btnFilterLP.setBounds (x + 2, y + sH / 2 - 8, 36, 16);
}
