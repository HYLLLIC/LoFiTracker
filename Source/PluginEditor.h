#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TrackerComponent.h"
#include "VoiceParamsPanel.h"

//==============================================================================
class LoFiTrackerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                          public juce::Timer
{
public:
    LoFiTrackerAudioProcessorEditor (LoFiTrackerAudioProcessor& p);
    ~LoFiTrackerAudioProcessorEditor() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;

    void timerCallback() override;  // sync UI ↔ engine state

private:
    LoFiTrackerAudioProcessor& processor;

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
