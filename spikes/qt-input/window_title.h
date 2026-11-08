#pragma once
#include <QObject>
#include <QWindow>

// Hides the window's title text while the title bar is folded into Band's top
// bar. Only macOS draws the title over the content then; the Linux adapter
// does nothing. Implemented in native/<platform>/window_title.*.
void bandSetWindowTitleHidden(QWindow *window, bool hidden);

class UiWindowTitle : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE void setHidden(QObject *window, bool hidden) { if (auto *w = qobject_cast<QWindow *>(window)) bandSetWindowTitleHidden(w, hidden); }
};
