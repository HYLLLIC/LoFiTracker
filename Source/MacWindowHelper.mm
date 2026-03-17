// .mm files are compiled only on macOS by Xcode — no #if JUCE_MAC guard needed.
// JUCE_MAC is defined by JUCE headers, so a top-level #if would evaluate to 0
// before any includes are processed, silently stripping the entire function body.

#import <Cocoa/Cocoa.h>
#include "MacWindowHelper.h"

void applyLightTitleBar (juce::ComponentPeer* peer)
{
    if (peer == nullptr) return;

    NSView* view = (NSView*) peer->getNativeHandle();
    if (view == nil) return;

    NSWindow* window = view.window;
    if (window == nil) return;

    // Force this window into Aqua (light) appearance even when the system
    // is in dark mode — keeps native traffic-light buttons and title text.
    if (@available (macOS 10.14, *))
        window.appearance = [NSAppearance appearanceNamed: NSAppearanceNameAqua];
}
