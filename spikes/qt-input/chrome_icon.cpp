#include "chrome_icon.h"
#include <QPainter>
#include <QPainterPath>
void ChromeIcon::paint(QPainter *p) {
    // Original geometric icons, independent of font fallback and terminal glyphs.
    p->setRenderHint(QPainter::Antialiasing);p->scale(width()/24,height()/24);
    p->setPen(QPen(color_,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->setBrush(Qt::NoBrush);
    if(name_=="menu") {for(int y:{5,12,19})p->drawLine(QPointF(4,y),QPointF(20,y));}
    else if(name_=="workspaces") {for(int y:{4,14})p->drawRoundedRect(QRectF(4,y,16,6),1,1);}
    else if(name_=="agents") {QPainterPath path;path.moveTo(2,12);path.lineTo(6,12);path.lineTo(9,5);path.lineTo(14,19);path.lineTo(18,12);path.lineTo(22,12);p->drawPath(path);}
    else if(name_=="close") {p->drawLine(QPointF(7,7),QPointF(17,17));p->drawLine(QPointF(17,7),QPointF(7,17));}
    else if(name_=="plus") {p->drawLine(QPointF(5,12),QPointF(19,12));p->drawLine(QPointF(12,5),QPointF(12,19));}
    else if(name_=="more") {p->setPen(Qt::NoPen);p->setBrush(color_);for(int x:{5,12,19})p->drawEllipse(QPointF(x,12),1,1);}
    else if(name_=="chevron") {p->drawLine(QPointF(7,10),QPointF(12,15));p->drawLine(QPointF(12,15),QPointF(17,10));}
    else if(name_=="pin") {p->drawLine(QPointF(8,4),QPointF(16,4));p->drawLine(QPointF(9,4),QPointF(9,10));p->drawLine(QPointF(15,4),QPointF(15,10));QPainterPath path;path.moveTo(9,10);path.lineTo(6,14);path.lineTo(18,14);path.lineTo(15,10);p->drawPath(path);p->drawLine(QPointF(12,14),QPointF(12,21));}
    else if(name_=="check") {p->drawLine(QPointF(5,12),QPointF(10,17));p->drawLine(QPointF(10,17),QPointF(20,6));}
    else if(name_=="help") {QPainterPath path;path.moveTo(8,8);path.cubicTo(8,3,17,3,17,8);path.cubicTo(17,12,12,11,12,15);p->drawPath(path);p->drawPoint(QPointF(12,20));}
}
