#pragma once
#include <QObject>
#include <QVariantMap>
#include <QLocalSocket>

// Explicit, bounded, read-only CAPS inspection. No PTY attachment or service start.
class UiRuntimeInfo final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap report READ report NOTIFY changed)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY changed)
    Q_PROPERTY(QString configuredRuntime READ configuredRuntime CONSTANT)
public:
    explicit UiRuntimeInfo(QString directory,QString configuredRuntime,QObject *parent=nullptr,int timeoutMs=1500);
    QVariantMap report() const {return report_;}
    QString configuredRuntime() const {return configuredRuntime_;}
    QString diagnostics() const;
    Q_INVOKABLE void inspect(const QString &identity);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool copyDiagnostics() const;
Q_SIGNALS:
    void changed();
private:
    void cancel();
    void finish(QLocalSocket *socket,const QVariantMap &value);
    void consume(QLocalSocket *socket);
    QString directory_,configuredRuntime_;
    int timeoutMs_;
    QLocalSocket *socket_=nullptr;
    QVariantMap report_;
};
