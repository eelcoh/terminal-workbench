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
