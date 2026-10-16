#include "terminal_composition.h"
#include <QTextCharFormat>
#include <algorithm>

void TerminalComposition::set(const QInputMethodEvent &event,const QFont &font)
{
    const auto text=event.preeditString();
    layout_.setText(text);layout_.setFont(font);
    QTextOption option;option.setWrapMode(QTextOption::NoWrap);layout_.setTextOption(option);
    cursor_=int(text.size());cursorVisible_=true;cursorColor_=Qt::white;
    bool explicitCursorColor=false;
    QList<QTextLayout::FormatRange> formats;
    // Supply an underline only where the engine has not supplied its own style.
    QTextCharFormat base;base.setFontUnderline(true);
    formats.append({0,int(text.size()),base});
    for(const auto &attribute:event.attributes()) {
        if(attribute.type==QInputMethodEvent::TextFormat) {
            const int start=std::clamp(attribute.start,0,int(text.size()));
            const int length=int(std::clamp<qint64>(qint64(attribute.start)+attribute.length,start,text.size()))-start;
            if(length>0 && attribute.value.canConvert<QTextFormat>())
                formats.append({start,length,attribute.value.value<QTextFormat>().toCharFormat()});
        } else if(attribute.type==QInputMethodEvent::Cursor) {
            cursor_=std::clamp(attribute.start,0,int(text.size()));
            cursorVisible_=attribute.length!=0;
            const auto color=attribute.value.value<QColor>();
            if(color.isValid()) {cursorColor_=color;explicitCursorColor=true;}
        }
    }
    if(!explicitCursorColor && !text.isEmpty()) {
        const int position=std::min(cursor_,int(text.size())-1);
        for(const auto &range:formats)
            if(position>=range.start && position<range.start+range.length && range.format.foreground().style()!=Qt::NoBrush)
                cursorColor_=range.format.foreground().color();
    }
    layout_.setFormats(formats);
    layout_.beginLayout();
    auto line=layout_.createLine();
    if(line.isValid())line.setLineWidth(1000000);
    layout_.endLayout();
}
QRectF TerminalComposition::caret(const QRectF &terminalCaret) const
{
    if(!active() || layout_.lineCount()==0)return terminalCaret;
    const auto line=layout_.lineAt(0);
    return QRectF(terminalCaret.x()+line.cursorToX(cursor_),terminalCaret.y(),1,terminalCaret.height());
}
void TerminalComposition::paint(QPainter *painter,const QRectF &terminalCaret,qreal ascent,const QColor &background) const
{
    if(!active() || layout_.lineCount()==0)return;
    const auto line=layout_.lineAt(0);
    const auto origin=terminalCaret.topLeft()+QPointF(0,ascent-line.ascent());
    const QRectF rect(origin,QSizeF(std::max(terminalCaret.width(),line.naturalTextWidth()),std::max(terminalCaret.height(),line.height())));
    painter->fillRect(rect,background);painter->setPen(Qt::white);
    layout_.draw(painter,origin);
    if(cursorVisible_) {
        painter->setPen(cursorColor_);
        const auto cursor=caret(terminalCaret);
        painter->drawLine(cursor.topLeft(),cursor.bottomLeft());
    }
}
