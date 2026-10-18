#pragma once
#include <QObject>
#include <QVariantMap>
class UiViewState final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap initial READ initial CONSTANT)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit UiViewState(const QString &path,QObject *parent=nullptr);
    QVariantMap initial() const {return initial_;}
    bool enabled() const {return !path_.isEmpty();}
    QString error() const {return error_;}
    Q_INVOKABLE bool save(const QVariantMap &state);
    Q_INVOKABLE void invalidate(const QString &message){writable_=false;error_=message;Q_EMIT changed();}
    static bool valid(const QVariantMap &state);
Q_SIGNALS:
    void changed();
private:
    QString path_,error_;QVariantMap initial_;QByteArray original_;bool writable_=true;
};
