#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QVariantList>

// Band's own host list (ui/hosts.json beside the font and shortcut settings),
// merged with [host.ID] from config.conf: for the same ID Band's entry wins.
// New host… suggests Host aliases from ~/.ssh/config, read only.
class UiHosts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList saved READ saved NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit UiHosts(QString path,QString sshConfig,QObject *parent=nullptr);
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
    static bool validId(const QString &id);
    static bool validDestination(const QString &destination);
    static QStringList aliases(const QByteArray &sshConfig);
Q_SIGNALS:
    void changed();
private:
    void load();
    bool write(const QVariantList &hosts);
    QString path_,sshConfig_,error_;QVariantList saved_;QFileSystemWatcher watcher_;
};
