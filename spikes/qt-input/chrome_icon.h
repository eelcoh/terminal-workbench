#pragma once
#include <QQuickPaintedItem>
#include <QColor>
#include <QImage>
// The platform's own symbol for a Band icon name, in one color; null when the
// platform has none (Linux, or a name without a system symbol).
QImage bandSystemSymbol(const QString &name,const QColor &color,const QSize &pixels);
class ChromeIcon : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY changed)
public:
    explicit ChromeIcon(QQuickItem *parent=nullptr):QQuickPaintedItem(parent){setAntialiasing(true);}
    QString name() const{return name_;}
    QColor color() const{return color_;}
    void setName(const QString &name){if(name_!=name){name_=name;update();Q_EMIT changed();}}
    void setColor(const QColor &color){if(color_!=color){color_=color;update();Q_EMIT changed();}}
    void paint(QPainter *painter) override;
Q_SIGNALS:
    void changed();
private:
    QString name_;QColor color_;
};
