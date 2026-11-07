#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QVariantList>

// Band's own host list (ui/hosts.json beside the font and shortcut settings),
// merged with [host.ID] from config.conf: for the same ID Band's entry wins.
// New host… suggests Host aliases from ~/.ssh/config, read only.
class UiHosts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList saved READ saved NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // ID → "checking", "reachable", "unreachable" or "unknown".
    Q_PROPERTY(QVariantMap reachability READ reachability NOTIFY reachabilityChanged)
public:
    explicit UiHosts(QString path,QString sshConfig,QObject *parent=nullptr,QString ssh=QStringLiteral("ssh"));
    QVariantList saved() const {return saved_;}
    QString error() const {return error_;}
    // Configured hosts with Band's saved hosts over them (marked "saved").
    Q_INVOKABLE QVariantList merge(const QVariantList &configured) const;
    // Adds or replaces a saved host; returns false with error set when invalid.
    Q_INVOKABLE bool save(const QVariantMap &host);
    Q_INVOKABLE bool remove(const QString &id);
    // An unused ID for a new host, from its name or destination.
    Q_INVOKABLE QString freeId(const QString &from,const QVariantList &taken) const;
    Q_INVOKABLE QStringList suggestions() const;
    QVariantMap reachability() const {return reachability_;}
    // Resolves each destination with `ssh -G` and tries a TCP connection to its
    // address and port (two seconds). Proxied hosts stay unknown; nothing logs in.
    Q_INVOKABLE void check(const QVariantList &hosts);
    // From `ssh -G` output: {host, port, proxied}.
    static QVariantMap resolved(const QByteArray &output);
    static bool validId(const QString &id);
    static bool validDestination(const QString &destination);
    static QStringList aliases(const QByteArray &sshConfig);
Q_SIGNALS:
    void changed();
    void reachabilityChanged();
private:
    void load();
    void mark(const QString &id,const QString &state);
    bool write(const QVariantList &hosts);
    QString path_,sshConfig_,ssh_,error_;QVariantList saved_;QFileSystemWatcher watcher_;QVariantMap reachability_;QSet<QString> pending_;
};
