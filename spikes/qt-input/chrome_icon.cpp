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
    else if(name_=="shell") {p->drawLine(QPointF(5,7),QPointF(10,12));p->drawLine(QPointF(10,12),QPointF(5,17));p->drawLine(QPointF(13,17),QPointF(19,17));}
    else if(name_=="filter-off") {QPainterPath path;path.moveTo(3,5);path.lineTo(17,5);path.lineTo(12,11);path.lineTo(12,17);path.lineTo(8,15);path.lineTo(8,11);path.closeSubpath();p->drawPath(path);p->drawLine(QPointF(15,15),QPointF(21,21));p->drawLine(QPointF(21,15),QPointF(15,21));}
    else if(name_=="apps") {for(int x:{4,14})for(int y:{4,14})p->drawRoundedRect(QRectF(x,y,6,6),1.5,1.5);}
    else if(name_=="width") {p->drawLine(QPointF(3,6),QPointF(3,18));p->drawLine(QPointF(21,6),QPointF(21,18));p->drawLine(QPointF(7,12),QPointF(17,12));
        for(int s:{1,-1}){const qreal x=s>0?7:17;p->drawLine(QPointF(x,12),QPointF(x+3*s,9));p->drawLine(QPointF(x,12),QPointF(x+3*s,15));}}
    else if(name_=="full") {for(int sx:{1,-1})for(int sy:{1,-1}){const QPointF c(12-8*sx,12-8*sy);p->drawLine(c,c+QPointF(5*sx,0));p->drawLine(c,c+QPointF(0,5*sy));}}
    else if(name_=="host") {p->drawEllipse(QPointF(12,12),8,8);p->drawEllipse(QPointF(12,12),3.5,8);p->drawLine(QPointF(4,12),QPointF(20,12));}
    else if(name_=="minimize") {p->drawLine(QPointF(6,16),QPointF(18,16));}
    else if(name_=="maximize") {p->drawRoundedRect(QRectF(6,6,12,12),1.5,1.5);}
    else if(name_=="restore") {p->drawRoundedRect(QRectF(6,9,9,9),1.5,1.5);p->drawLine(QPointF(9,6),QPointF(18,6));p->drawLine(QPointF(18,6),QPointF(18,15));}
    else if(name_=="close") {p->drawLine(QPointF(7,7),QPointF(17,17));p->drawLine(QPointF(17,7),QPointF(7,17));}
    else if(name_=="plus") {p->drawLine(QPointF(5,12),QPointF(19,12));p->drawLine(QPointF(12,5),QPointF(12,19));}
    else if(name_=="more") {p->setPen(Qt::NoPen);p->setBrush(color_);for(int x:{5,12,19})p->drawEllipse(QPointF(x,12),1,1);}
    else if(name_=="chevron") {p->drawLine(QPointF(7,10),QPointF(12,15));p->drawLine(QPointF(12,15),QPointF(17,10));}
    else if(name_=="pin") {p->drawLine(QPointF(8,4),QPointF(16,4));p->drawLine(QPointF(9,4),QPointF(9,10));p->drawLine(QPointF(15,4),QPointF(15,10));QPainterPath path;path.moveTo(9,10);path.lineTo(6,14);path.lineTo(18,14);path.lineTo(15,10);p->drawPath(path);p->drawLine(QPointF(12,14),QPointF(12,21));}
    else if(name_=="check") {p->drawLine(QPointF(5,12),QPointF(10,17));p->drawLine(QPointF(10,17),QPointF(20,6));}
    else if(name_=="info") {p->drawEllipse(QPointF(12,12),9,9);p->drawPoint(QPointF(12,7));p->drawLine(QPointF(12,11),QPointF(12,17));}
    else if(name_=="help") {QPainterPath path;path.moveTo(8,8);path.cubicTo(8,3,17,3,17,8);path.cubicTo(17,12,12,11,12,15);p->drawPath(path);p->drawPoint(QPointF(12,20));}
}
