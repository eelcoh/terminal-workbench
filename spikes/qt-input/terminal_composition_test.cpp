#include "terminal_composition.h"
#include <QImage>
#include <QTextCharFormat>
#include <QFontMetricsF>
#include <cstdio>

bool terminalCompositionSelfTest()
{
    TerminalComposition composition;
    QFont font("monospace");font.setPixelSize(18);
    const QRectF terminalCaret(8,8,12,24);
    QTextCharFormat selected;
    selected.setBackground(Qt::cyan);selected.setForeground(Qt::black);selected.setFontUnderline(false);
    // Qt offsets are UTF-16: the emoji occupies two units, the next character starts at 3.
    const auto text=QString::fromUtf8("a😀に");
    auto event=[&](int position,int visible) {
        return QInputMethodEvent(text,{
            {QInputMethodEvent::TextFormat,1,2,QVariant::fromValue(QTextFormat(selected))},
            {QInputMethodEvent::Cursor,position,visible,QColor(Qt::magenta)}});
    };
    auto render=[&] {
        QImage image(200,70,QImage::Format_RGB32);image.fill(Qt::black);
        QPainter painter(&image);composition.paint(&painter,terminalCaret,QFontMetricsF(font).ascent(),Qt::black);
        painter.end();return image;
    };
    auto pixels=[](const QImage &image,QColor color) {
        int count=0;
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
            if(image.pixelColor(x,y)==color)++count;
        return count;
    };
    auto require=[](bool value,const char *message) {
        if(!value)std::fprintf(stderr,"IME overlay self-test: %s\n",message);
        return value;
    };
    auto start=event(0,1);composition.set(start,font);
    if(!require(composition.caret(terminalCaret).x()==terminalCaret.x(),"cursor at start"))return false;
    auto middle=event(3,1);composition.set(middle,font);
    const auto middleX=composition.caret(terminalCaret).x();
    auto image=render();
    if(!require(pixels(image,Qt::cyan)>0,"engine segment background rendered") ||
       !require(pixels(image,Qt::magenta)>0,"engine cursor color rendered"))return false;
    auto end=event(4,0);composition.set(end,font);image=render();
    if(!require(middleX>terminalCaret.x() && composition.caret(terminalCaret).x()>middleX,"UTF-16 cursor after emoji and before final character") ||
       !require(pixels(image,Qt::magenta)==0,"hidden composition cursor") ||
       !require(pixels(image,Qt::cyan)>0,"segment retained with hidden cursor"))return false;
    QInputMethodEvent clear;composition.set(clear,font);
    if(!require(!composition.active() && composition.caret(terminalCaret)==terminalCaret,"cancel restores terminal caret") ||
       !require(pixels(render(),Qt::cyan)==0,"cancel clears segment styling"))return false;
    QInputMethodEvent plain(text,{});composition.set(plain,font);
    if(!require(composition.caret(terminalCaret).x()>middleX,"missing cursor attribute defaults to end"))return false;
    std::puts("PASS: IME segment rendering, UTF-16 caret, cursor visibility and cancel cleanup");
    return true;
}
