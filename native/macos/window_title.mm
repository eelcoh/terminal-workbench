#include "window_title.h"
#import <AppKit/AppKit.h>

// With ExpandedClientAreaHint and NoTitleBarBackgroundHint the title bar has no
// background but AppKit still draws the title; Band's top bar names the
// workspace instead, so the title text is hidden. Mission Control and the
// Window menu keep the title.
void bandSetWindowTitleHidden(QWindow *window, bool hidden)
{
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    if (!view || !view.window) return;
    view.window.titleVisibility = hidden ? NSWindowTitleHidden : NSWindowTitleVisible;
    view.window.titlebarAppearsTransparent = hidden;
}

// The system's sidebar material behind the Qt content: an NSVisualEffectView
// under Qt's view, blurring what is behind the window. Qt draws its top bar
// and sidebar with a translucent tint over it and everything else opaque, so
// the window itself must be transparent (Ui.qml sets its color). The material
// follows the terminal theme's light or dark, not the system's, and AppKit
// makes it opaque under Reduce Transparency.
bool bandSetWindowMaterial(QWindow *window, bool dark, bool on)
{
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    if (!view || !view.window || !view.superview) return false;
    NSView *frame = view.superview;
    NSVisualEffectView *material = nil;
    for (NSView *sibling in frame.subviews)
        if ([sibling.identifier isEqualToString:@"band-material"]) material = static_cast<NSVisualEffectView *>(sibling);
    // Off: the view stays in place, hidden; the client then paints the whole window.
    if (!on) { material.hidden = YES; return false; }
    if (!material) {
        material = [[NSVisualEffectView alloc] initWithFrame:frame.bounds];
        material.identifier = @"band-material";
        material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        material.material = NSVisualEffectMaterialSidebar;
        material.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        material.state = NSVisualEffectStateFollowsWindowActiveState;
        [frame addSubview:material positioned:NSWindowBelow relativeTo:view];
    }
    material.hidden = NO;
    material.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
    view.window.opaque = NO;
    view.window.backgroundColor = NSColor.clearColor;
    return true;
}
