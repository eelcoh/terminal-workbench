#pragma once
#include <QObject>
#include <QVariantMap>
#include <memory>

// Client presentation; managed settings remain read-only. Apply saves a separate UI layer.
class UiFonts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantMap defaults READ defaults NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString editError READ editError NOTIFY changed)
public:
    explicit UiFonts(const QString &path,bool apple,QObject *parent=nullptr,const QString &overrides={});
    std::unique_ptr<UiFonts> reloadCandidate() const;
    // Coordinator publishes both adapters only after the entire candidate validates.
    void adoptReload(const UiFonts &candidate);
    QString managedPath() const {return path_;}
    QString overridesPath() const {return overrides_;}
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
    QVariantMap current_,defaults_,original_,persisted_;QString error_,editError_,path_,overrides_;
    QByteArray managedBytes_,overrideBytes_;bool saveAllowed_=true,apple_=false;
};
