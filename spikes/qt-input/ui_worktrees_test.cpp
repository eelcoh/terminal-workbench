#include "ui_worktrees.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
#include <functional>
static void require(bool result,const char *why) {if(!result){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static bool wait(const std::function<bool()> &condition,int timeout=10000) {
    QElapsedTimer timer;timer.start();while(!condition() && timer.elapsed()<timeout)QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    return condition();
}
static void git(const QString &directory,const QStringList &arguments) {
    QProcess process;process.start("git",QStringList{"-C",directory,"-c","user.name=Test","-c","user.email=test@example.invalid"}+arguments);
    require(process.waitForFinished(10000) && process.exitCode()==0,"git fixture command");
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    const auto parsed=UiWorktrees::parse("worktree /a/main\nHEAD 0123\nbranch refs/heads/main\n\nworktree /a/topic\nHEAD 4567\nbranch refs/heads/feature/x\n\nworktree /a/loose\nHEAD 89ab\ndetached\n\nworktree /a/bare\nbare\n");
    require(parsed.size()==3 && parsed[0].toMap()["main"].toBool() && parsed[1].toMap()["branch"]=="feature/x" && parsed[2].toMap()["branch"].toString().isEmpty(),"porcelain worktrees, main first, detached and bare");
    for(const auto *good:{"topic","feature/x","fix-12"})require(UiWorktrees::validBranch(good),"valid branch name");
    for(const auto *bad:{"","-rf","a b","a..b","a~1","x.lock","/x","x/","a//b","@","a:b","a\\b"})require(!UiWorktrees::validBranch(bad),"invalid branch name rejected");
    QProcess probe;probe.start("git",{"--version"});
    if(!probe.waitForFinished(5000) || probe.exitCode()!=0){std::puts("SKIP: git not available");return 0;}

    QTemporaryDir temp;require(temp.isValid(),"temporary root");
    const auto repo=temp.filePath("Repo café");QDir().mkpath(repo);
    git(repo,{"init","-q","-b","main"});git(repo,{"commit","-q","--allow-empty","-m","fixture"});git(repo,{"branch","existing"});
    QDir().mkpath(repo+"/sub");
    UiWorktrees worktrees;QStringList added;
    QObject::connect(&worktrees,&UiWorktrees::added,&app,[&](const QString &path){added.append(path);});
    worktrees.inspect("Repo café/sub",temp.path());
    require(wait([&]{return !worktrees.repository().isEmpty();}),"relative folder inside a repository");
    require(QFileInfo(worktrees.repository()).canonicalFilePath()==QFileInfo(repo).canonicalFilePath() && worktrees.worktrees().size()==1,"main worktree found from a subfolder");
    require(worktrees.pathFor("feature/x").endsWith("/Repo café-feature-x"),"new worktrees go beside the main checkout");
    worktrees.add("feature/x");
    require(wait([&]{return added.size()==1;}) && QFileInfo(added[0]+"/.git").exists() && worktrees.error().isEmpty(),"new branch worktree added");
    worktrees.add("existing");
    require(wait([&]{return added.size()==2;}) && QFileInfo(added[1]+"/.git").exists(),"existing branch checked out in a worktree");
    worktrees.inspect(repo);
    require(wait([&]{return worktrees.worktrees().size()==3;}),"worktree list refreshed");
    worktrees.add("feature/x");
    require(wait([&]{return added.size()==3;}) && added[2]==added[0],"a branch with a worktree reuses it");
    worktrees.add("bad name");require(!worktrees.error().isEmpty() && !worktrees.busy(),"invalid branch reported without running git");
    worktrees.add("main");
    require(wait([&]{return !worktrees.busy();}) && added.size()==4 && QFileInfo(added[3]).canonicalFilePath()==QFileInfo(repo).canonicalFilePath(),"the main branch reuses the main checkout");
    git(repo,{"branch","taken"});QDir().mkpath(worktrees.pathFor("taken"));
    worktrees.add("taken");require(worktrees.error().contains("already exists") && !worktrees.busy(),"an existing folder is not overwritten");
    worktrees.inspect(temp.filePath("missing"));require(wait([&]{return worktrees.repository().isEmpty();}),"a missing folder has no repository");
    std::puts("PASS: worktree porcelain, branch names, relative folders, new and existing branches, reuse, existing folders, missing folders");
}
