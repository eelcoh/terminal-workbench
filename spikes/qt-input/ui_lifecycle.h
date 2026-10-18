#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QVariantMap>
#include <QHash>
#include <QSet>
// Read-only, event/request driven. Never starts a service or retries a mutation.
class UiLifecycle final : public QObject {
    Q_OBJECT
public:
    explicit UiLifecycle(QString endpoint,QObject *parent=nullptr,int timeoutMs=3000):QObject(parent),endpoint_(std::move(endpoint)),timeoutMs_(timeoutMs){}
    QVariantMap values() const{return lifecycle_;}
    void inspect(const QString &identity);
    void retain(const QSet<QString> &identities);
Q_SIGNALS:
    void changed();
private:
    QString endpoint_;int timeoutMs_;
    QVariantMap lifecycle_;QHash<QString,QLocalSocket *> inspections_;
};
