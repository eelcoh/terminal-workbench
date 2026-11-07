#include "ui_worktrees.h"
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

QProcess *UiWorktrees::run(const QString &directory,const QStringList &arguments) {
    auto *process=new QProcess(this);
    auto environment=QProcessEnvironment::systemEnvironment();
    environment.insert("GIT_TERMINAL_PROMPT","0");environment.insert("GIT_OPTIONAL_LOCKS","0");
    process->setProcessEnvironment(environment);
    process->setProgram(git_);
    process->setArguments(QStringList{"-c","core.fsmonitor=false","-C",directory}+arguments);
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process,&QProcess::finished,process,&QObject::deleteLater);
    connect(process,&QProcess::errorOccurred,process,[process](QProcess::ProcessError error){if(error==QProcess::FailedToStart)process->deleteLater();});
    return process;
}
QVariantList UiWorktrees::parse(const QByteArray &porcelain) {
    QVariantList result;QVariantMap current;
    const auto flush=[&]{if(!current.value("path").toString().isEmpty() && !current.value("bare").toBool())result.append(current);current.clear();};
    for(const auto &line:porcelain.split('\n')) {
        const auto text=QString::fromUtf8(line);
        if(text.isEmpty()){flush();continue;}
        if(text.startsWith("worktree ")){flush();current.insert("path",text.mid(9));current.insert("main",result.isEmpty());}
        else if(text.startsWith("branch refs/heads/"))current.insert("branch",text.mid(18));
        else if(text=="detached")current.insert("branch",QString());
        else if(text=="bare")current.insert("bare",true);
    }
    flush();return result;
}
void UiWorktrees::clear() {
    if(lookup_)lookup_->kill();
    if(repository_.isEmpty() && worktrees_.isEmpty() && error_.isEmpty())return;
    repository_.clear();worktrees_.clear();error_.clear();Q_EMIT changed();
}
void UiWorktrees::inspect(const QString &folder,const QString &base) {
    if(adding_)return;
    if(lookup_)lookup_->kill();
    QString path=folder.trimmed();
    if(path.startsWith("~/"))path=QDir::homePath()+path.mid(1);
    if(path.isEmpty() || !QFileInfo(QDir(base.isEmpty()?QDir::currentPath():base).absoluteFilePath(path)).isDir()){clear();return;}
    path=QDir(base.isEmpty()?QDir::currentPath():base).absoluteFilePath(path);
    auto *process=run(path,{"worktree","list","--porcelain"});lookup_=process;
    connect(process,&QProcess::finished,this,[this,process](int code,QProcess::ExitStatus status){
        if(process!=lookup_)return;
        const auto list=code==0 && status==QProcess::NormalExit ? parse(process->readAllStandardOutput()) : QVariantList{};
        const auto main=list.isEmpty() ? QString() : list.first().toMap().value("path").toString();
        if(main==repository_ && list==worktrees_)return;
        repository_=main;worktrees_=list;error_.clear();Q_EMIT changed();
    });
    connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error){if(error==QProcess::FailedToStart && process==lookup_)clear();});
    process->start();
}
bool UiWorktrees::validBranch(const QString &branch) {
    // A conservative subset of git check-ref-format; Git still has the last word.
    static const QRegularExpression invalid(QStringLiteral(R"([\x00-\x20\x7f~^:?*\[\\]|\.\.|@\{|//|/\.|\.lock$|\.lock/|^[-/.]|[/.]$)"));
    return !branch.isEmpty() && branch!="@" && branch.size()<=200 && !invalid.match(branch).hasMatch();
}
QString UiWorktrees::pathFor(const QString &branch) const {
    if(repository_.isEmpty() || !validBranch(branch))return {};
    const QFileInfo main(repository_);
    return QDir(main.absolutePath()).absoluteFilePath(main.fileName()+"-"+QString(branch).replace('/','-'));
}
void UiWorktrees::fail(const QString &message) {adding_=false;error_=message;Q_EMIT changed();}
void UiWorktrees::add(const QString &requested) {
    const auto branch=requested.trimmed();
    if(adding_ || repository_.isEmpty())return;
    if(!validBranch(branch)){fail("Choose a branch name without spaces or special characters.");return;}
    for(const auto &item:worktrees_)if(item.toMap().value("branch").toString()==branch){adding_=false;error_.clear();Q_EMIT changed();Q_EMIT added(item.toMap().value("path").toString());return;}
    const auto path=pathFor(branch);
    if(QFileInfo::exists(path)){fail("A folder already exists at "+path+".");return;}
    adding_=true;error_.clear();branch_=branch;Q_EMIT changed();
    // An existing branch is checked out; otherwise it is created from HEAD.
    auto *check=run(repository_,{"rev-parse","--verify","--quiet","refs/heads/"+branch});
    connect(check,&QProcess::finished,this,[this,path](int code,QProcess::ExitStatus status){
        const bool exists=code==0 && status==QProcess::NormalExit;
        auto *create=run(repository_,exists ? QStringList{"worktree","add","--",path,branch_} : QStringList{"worktree","add","-b",branch_,"--",path});
        connect(create,&QProcess::finished,this,[this,create,path](int code,QProcess::ExitStatus status){
            if(code!=0 || status!=QProcess::NormalExit){
                const auto message=QString::fromUtf8(create->readAllStandardError()).trimmed().section('\n',-1).left(300);
                fail(message.isEmpty() ? QStringLiteral("Git could not add the worktree.") : message);return;
            }
            adding_=false;Q_EMIT changed();Q_EMIT added(path);
        });
        connect(create,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart)fail("Git is not available.");});
        create->start();
    });
    connect(check,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart)fail("Git is not available.");});
    check->start();
}
