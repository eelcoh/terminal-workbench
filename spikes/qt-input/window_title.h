#pragma once
#include <QObject>
#include <QWindow>

// Hides the window's title text while the title bar is folded into Band's top
// bar. Only macOS draws the title over the content then; the Linux adapter
// does nothing. Implemented in native/<platform>/window_title.*.
void bandSetWindowTitleHidden(QWindow *window, bool hidden);
// Puts the system's sidebar material behind the window's content, in the
// light or dark variant, and reports whether it is there. macOS only; the
// Linux adapter returns false and the window stays opaque.
bool bandSetWindowMaterial(QWindow *window, bool dark, bool on);

class UiWindowTitle : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool setMaterial(QObject *window, bool dark, bool on) { auto *w = qobject_cast<QWindow *>(window); return w && bandSetWindowMaterial(w, dark, on); }
    Q_INVOKABLE void setHidden(QObject *window, bool hidden) { if (auto *w = qobject_cast<QWindow *>(window)) bandSetWindowTitleHidden(w, hidden); }
};
