#pragma once
#include <QObject>
#include <QVariantMap>

// Window-local presentation. Managed config is read once; it is never written.
class UiFonts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantMap defaults READ defaults CONSTANT)
    Q_PROPERTY(QString error READ error CONSTANT)
public:
    explicit UiFonts(const QString &path,bool apple,QObject *parent=nullptr);
    QVariantMap current() const {return current_;}
    QVariantMap defaults() const {return defaults_;}
    QString error() const {return error_;}
    Q_INVOKABLE bool preview(const QString &family,int size);
    Q_INVOKABLE void begin() {original_=current_;}
    Q_INVOKABLE void apply() {original_=current_;}
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void useDefaults();
    Q_INVOKABLE void step(int direction);
    Q_INVOKABLE void resetSize();
Q_SIGNALS:
    void changed();
private:
    QVariantMap current_,defaults_,original_;QString error_;
};
