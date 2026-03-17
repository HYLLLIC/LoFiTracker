#pragma once
#include <JuceHeader.h>

// Forces the host OS window into Aqua (light) appearance so the native
// macOS title bar is white with black text and standard traffic-light
// buttons, regardless of the system dark/light mode setting.
// No-op on non-macOS platforms.

#if JUCE_MAC
void applyLightTitleBar (juce::ComponentPeer* peer);
#endif
