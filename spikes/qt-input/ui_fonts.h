#pragma once
#include <QObject>
#include <QVariantMap>

// Client presentation; managed settings remain read-only. Apply saves a separate UI layer.
class UiFonts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantMap defaults READ defaults CONSTANT)
    Q_PROPERTY(QString error READ error CONSTANT)
    Q_PROPERTY(QString editError READ editError NOTIFY changed)
public:
    explicit UiFonts(const QString &path,bool apple,QObject *parent=nullptr,const QString &overrides={});
    QVariantMap current() const {return current_;}
    QVariantMap defaults() const {return defaults_;}
    QString error() const {return error_;}
    QString editError() const {return editError_;}
    Q_INVOKABLE bool preview(const QString &family,int size);
    Q_INVOKABLE void begin() {original_=current_;editError_.clear();Q_EMIT changed();}
    Q_INVOKABLE bool apply();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void useDefaults();
    Q_INVOKABLE void step(int direction);
    Q_INVOKABLE void resetSize();
Q_SIGNALS:
    void changed();
private:
    QVariantMap current_,defaults_,original_;QString error_,editError_,path_,overrides_;
    QByteArray managedBytes_,overrideBytes_;bool saveAllowed_=true;
};
