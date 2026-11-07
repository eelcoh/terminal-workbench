#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QVariantList>

// Git worktrees for the New workspace form: the repository a folder belongs to,
// its worktrees, and adding one on a branch beside the main checkout. Git runs
// asynchronously with fsmonitor off and no terminal prompt.
class UiWorktrees final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString repository READ repository NOTIFY changed)
    Q_PROPERTY(QVariantList worktrees READ worktrees NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit UiWorktrees(QObject *parent=nullptr,QString git=QStringLiteral("git")):QObject(parent),git_(std::move(git)) {}
    QString repository() const {return repository_;}
    QVariantList worktrees() const {return worktrees_;}
    bool busy() const {return adding_;}
    QString error() const {return error_;}
    // Looks up the repository of `folder` (relative folders resolve against `base`).
    Q_INVOKABLE void inspect(const QString &folder,const QString &base={});
    Q_INVOKABLE void clear();
    // Adds a worktree for `branch` (created from HEAD when it does not exist yet).
    Q_INVOKABLE void add(const QString &branch);
    Q_INVOKABLE QString pathFor(const QString &branch) const;
    static bool validBranch(const QString &branch);
    static QVariantList parse(const QByteArray &porcelain);
Q_SIGNALS:
    void changed();
    void added(const QString &path);
private:
    QProcess *run(const QString &directory,const QStringList &arguments);
    void fail(const QString &message);
    QString git_,repository_,error_,branch_;QVariantList worktrees_;bool adding_=false;
    QPointer<QProcess> lookup_;
};
