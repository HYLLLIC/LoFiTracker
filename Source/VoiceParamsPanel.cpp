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

    // Section labels
    addLbl (lblFMSection,      "--- FM ---");
    lblFMSection.setColour      (juce::Label::textColourId, colAccent);
    addLbl (lblCarrierSection, "--- CARRIER ---");
    lblCarrierSection.setColour (juce::Label::textColourId, colAccent);
    addLbl (lblModSection,     "--- MODULATOR ---");
    lblModSection.setColour     (juce::Label::textColourId, colAccent);
    addLbl (lblLoFiSection,    "--- LO-FI ---");
    lblLoFiSection.setColour    (juce::Label::textColourId, colAccent);

    // FM (no log skew needed)
    buildSlider (slModRatio, lblModRatio, "Ratio",  0.125, 16.0, 2.0);
    buildSlider (slModIndex, lblModIndex, "Index",  0.0,   20.0, 5.0);

    // Carrier envelope — log scale for time params (midPoint = 0.05 s)
    buildSlider (slCAtk, lblCAtk, "C.Atk", 0.001, 2.0,  0.001, false, 0.05);
    buildSlider (slCDcy, lblCDcy, "C.Dcy", 0.001, 4.0,  0.25,  false, 0.05);
    buildSlider (slCSus, lblCSus, "C.Sus", 0.0,   1.0,  0.0);
    buildSlider (slCRel, lblCRel, "C.Rel", 0.001, 2.0,  0.05,  false, 0.05);

    // Modulator envelope — log scale for time params
    buildSlider (slMAtk, lblMAtk, "M.Atk", 0.001, 2.0,  0.001, false, 0.05);
    buildSlider (slMDcy, lblMDcy, "M.Dcy", 0.001, 4.0,  0.12,  false, 0.05);
    buildSlider (slMSus, lblMSus, "M.Sus", 0.0,   1.0,  0.0);
    buildSlider (slMRel, lblMRel, "M.Rel", 0.001, 2.0,  0.05,  false, 0.05);

    // Lo-fi
    buildSlider (slBitDepth,  lblBitDepth,  "Bits",   1.0, 16.0, 8.0, true);
    buildSlider (slSrDiv,     lblSrDiv,     "SR/",    1.0, 8.0,  1.0, true);
    buildSlider (slFilterCut, lblFilterCut, "Filter", 0.0, 1.0,  0.5);
    buildSlider (slVolume,    lblVolume,    "Volume", 0.0, 1.0,  0.8);

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
                                     double lo, double hi, double def,
                                     bool isInt, double midPoint)
{
    addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setRange (lo, hi, isInt ? 1.0 : 0.0);
    if (midPoint > 0.0)
        s.setSkewFactorFromMidPoint (midPoint);
    s.setValue (def, juce::dontSendNotification);
    // Width is updated in resized() to match the actual dial column width
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);

    // ---- Compact value formatters ----
    if (isInt)
    {
        s.textFromValueFunction = [] (double v) {
            return juce::String (juce::roundToInt (v));
        };
        s.valueFromTextFunction = [] (const juce::String& t) {
            return t.getDoubleValue();
        };
    }
    else if (midPoint > 0.0)   // time/envelope values — show in ms or s
    {
        s.textFromValueFunction = [] (double v) -> juce::String {
            if (v < 1.0)
                return juce::String (juce::roundToInt (v * 1000.0)) + "ms";
            return juce::String (v, 2) + "s";
        };
        s.valueFromTextFunction = [] (const juce::String& t) -> double {
            const auto trimmed = t.trimEnd();
            if (trimmed.endsWithIgnoreCase ("ms"))
                return trimmed.dropLastCharacters (2).getDoubleValue() / 1000.0;
            if (trimmed.endsWithIgnoreCase ("s"))
                return trimmed.dropLastCharacters (1).getDoubleValue();
            return trimmed.getDoubleValue();
        };
    }
    else   // ratio, index, filter cutoff, volume — fixed decimal
    {
        s.textFromValueFunction = [] (double v) -> juce::String {
            if (v >= 10.0) return juce::String (v, 1);
            return juce::String (v, 2);
        };
        s.valueFromTextFunction = [] (const juce::String& t) {
            return t.getDoubleValue();
        };
    }

    // ---- Rotary colours ----
    s.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xff4C7030));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a2a28));
    s.setColour (juce::Slider::thumbColourId,               juce::Colour (0xffFDC618));
    s.setColour (juce::Slider::textBoxTextColourId,         colText);
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (0xff141412));
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colour (0xff1a1a18));
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
    g.setColour (juce::Colour (0xff4C7030).withAlpha (0.4f));  // olive top border
    g.drawRect (getLocalBounds(), 1);
}

void VoiceParamsPanel::resized()
{
    const int w       = getWidth();
    const int h       = getHeight();
    const int lblH    = 13;
    const int secLblH = 12;

    // 14 dials + inter-section gaps
    const int kGap    = 8;
    const int slW     = juce::jmax (24, (w - 4 - 13 * 2 - 3 * kGap) / 14);

    const int y  = secLblH + 4;
    const int sH = h - y - lblH - 2;

    int x = 2;

    // Update text box width to match actual dial width, then place
    auto place = [&] (juce::Slider& s, juce::Label& l)
    {
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, slW - 2, 14);
        l.setBounds (x, h - lblH, slW, lblH);
        s.setBounds (x, y,        slW, sH);
        x += slW + 2;
    };

    // FM section
    const int xFM = x;
    place (slModRatio, lblModRatio);
    place (slModIndex, lblModIndex);
    x += kGap;

    // Carrier envelope section
    const int xCarrier = x;
    place (slCAtk, lblCAtk);
    place (slCDcy, lblCDcy);
    place (slCSus, lblCSus);
    place (slCRel, lblCRel);
    x += kGap;

    // Modulator envelope section
    const int xMod = x;
    place (slMAtk, lblMAtk);
    place (slMDcy, lblMDcy);
    place (slMSus, lblMSus);
    place (slMRel, lblMRel);
    x += kGap;

    // Lo-fi section
    const int xLoFi = x;
    place (slBitDepth,  lblBitDepth);
    place (slSrDiv,     lblSrDiv);
    place (slFilterCut, lblFilterCut);
    place (slVolume,    lblVolume);

    btnFilterLP.setBounds (x + 2, y + sH / 2 - 8, 36, 16);

    // Section labels — positioned from actual slider start x values
    const int secW = slW * 2 + 2;   // width for 2-dial sections
    const int env4W = slW * 4 + 6;  // width for 4-dial sections
    lblFMSection.setBounds      (xFM,      2, secW,  secLblH);
    lblCarrierSection.setBounds (xCarrier, 2, env4W, secLblH);
    lblModSection.setBounds     (xMod,     2, env4W, secLblH);
    lblLoFiSection.setBounds    (xLoFi,    2, env4W, secLblH);
}
