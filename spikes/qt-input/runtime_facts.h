#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QVariantMap>
#include <functional>

// Read-only, independently bounded metadata subscriptions. Never attaches a PTY.
class RuntimeFeed final : public QObject {
public:
    RuntimeFeed(QByteArray request,quint8 kind,QObject *parent,
                std::function<bool(const QByteArray &)> consume,std::function<void()> unavailable);
    ~RuntimeFeed() override;
    void open(const QString &path);
private:
    void drain();
    void fail();
    QByteArray request_,buffer_;quint8 kind_;QLocalSocket socket_;QTimer deadline_;
    std::function<bool(const QByteArray &)> consume_;std::function<void()> unavailable_;
    bool failed_=true;
};
class RuntimeFacts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap facts READ facts NOTIFY changed)
public:
    explicit RuntimeFacts(const QString &path,QObject *parent=nullptr);
    QVariantMap facts() const;
    void refresh();
Q_SIGNALS:
    void changed();
private:
    bool context(const QByteArray &payload);
    bool agent(const QByteArray &payload);
    void clearContext();
    void clearAgent();
    QString path_,cwd_,repository_,branch_,worktree_;
    bool contextAvailable_=false,remote_=false,detached_=false,agentAvailable_=false;
    quint8 agent_=255,state_=0;
    RuntimeFeed contextFeed_,agentFeed_;
};
