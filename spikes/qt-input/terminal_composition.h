#pragma once
#include <QInputMethodEvent>
#include <QTextLayout>
#include <QPainter>

// Client-only IME overlay. Layout is rebuilt on composition events, never paint.
class TerminalComposition {
public:
    void set(const QInputMethodEvent &,const QFont &);
    bool active() const { return !layout_.text().isEmpty(); }
    QString text() const { return layout_.text(); }
    QRectF caret(const QRectF &terminalCaret) const;
    void paint(QPainter *,const QRectF &terminalCaret,qreal ascent,const QColor &background) const;
private:
    QTextLayout layout_;
    int cursor_=0;
    bool cursorVisible_=true;
    QColor cursorColor_=Qt::white;
};
bool terminalCompositionSelfTest();
