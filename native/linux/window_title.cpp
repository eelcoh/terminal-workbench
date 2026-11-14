#include "window_title.h"

// Linux uses a frameless window when the title bar is folded in; there is no
// system title to hide.
void bandSetWindowTitleHidden(QWindow *, bool) {}

// No system material on Linux: the window stays opaque.
bool bandSetWindowMaterial(QWindow *, bool, bool) { return false; }
