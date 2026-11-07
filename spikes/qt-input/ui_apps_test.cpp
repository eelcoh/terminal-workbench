#include "ui_apps.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char *text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static void executable(const QString &path,const QByteArray &body){QFile file(path);require(file.open(QIODevice::WriteOnly) && file.write(body)==body.size(),"write fixture");file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);}
static void check(UiApps &apps){
    QElapsedTimer timer;timer.start();bool done=false;
    auto connection=QObject::connect(&apps,&UiApps::checked,[&]{done=true;});
    // A check already running finishes first; wait for the one asked for here.
    apps.refresh();while((!done || apps.checking()) && timer.elapsed()<20000)QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents,50);
    QObject::disconnect(connection);require(done,"probe finished");
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir root;require(root.isValid(),"private directory");
    // Only the fixture's programs are on PATH. The shell is not a known login
    // shell, so it runs `-c env` without -l and profile files cannot add to PATH.
    const QString bin=root.filePath("bin"),shell=root.filePath("fakeshell"),marker=root.filePath("ostree-booted");
    require(QDir().mkpath(bin) && QFile::link("/usr/bin/env",bin+"/env"),"fixture PATH");
    executable(shell,"#!/bin/sh\nexec /bin/sh \"$@\"\n");
    for(const auto *name:{"lazygit","mine","dnf"})executable(bin+"/"+name,"#!/bin/sh\nexit 0\n");
    qputenv("PATH",bin.toLocal8Bit());

    require(UiApps::program("btop --utf-force")=="btop" && UiApps::program("FOO=1 btop").isEmpty() && UiApps::program("$HOME/x").isEmpty(),"program word");
    require(UiApps::program("~/bin/tool")==QDir::home().filePath("bin/tool"),"home program");
    require(UiApps::probeScript({"it's"},marker).contains("'it'\\''s'"),"quoted program");

    UiApps onLinux(false,shell,nullptr,marker);
    require(onLinux.entries().size()==UiApps::catalog().size() && onLinux.entry("lazygit")["installed"].toInt()==-1,"unchecked catalog");
    onLinux.setDefinitions({QVariantMap{{"id","mine"},{"name","Mine"},{"command","mine --flag"},{"directory",""}},
        QVariantMap{{"id","lazygit"},{"name","LG"},{"command","lazygit -p ~"},{"directory","~"}}});
    check(onLinux);
    require(onLinux.entries().first().toMap()["id"]=="mine" && onLinux.entry("mine")["installed"].toInt()==1 && !onLinux.entry("mine")["builtin"].toBool(),"own definition first");
    require(onLinux.entry("lazygit")["name"]=="LG" && onLinux.entry("lazygit")["command"]=="lazygit -p ~" && onLinux.entry("lazygit")["installed"].toInt()==1,"definition overrides a catalog entry");
    require(onLinux.entry("lazygit")["description"].toString().contains("Git"),"catalog description kept");
    require(onLinux.entry("btop")["installed"].toInt()==0 && onLinux.entry("btop")["install"]=="sudo dnf install btop" && onLinux.entry("btop")["via"]=="dnf","system package");
    require(onLinux.entry("yazi")["install"].toString().isEmpty() && onLinux.entry("yazi")["hint"].toString().contains("Homebrew"),"not packaged");

    onLinux.setDefinitions({QVariantMap{{"id","later"},{"name","Later"},{"command","later"},{"directory",""}}});
    require(onLinux.entry("later")["installed"].toInt()==-1,"unknown until checked, not missing");
    executable(bin+"/brew","#!/bin/sh\nexit 0\n");check(onLinux);
    require(onLinux.entry("btop")["install"]=="brew install btop" && onLinux.entry("yazi")["install"]=="brew install yazi","Homebrew first on Linux");
    QFile::remove(bin+"/brew");executable(marker,"");check(onLinux);
    require(onLinux.entry("btop")["install"].toString().isEmpty() && onLinux.entry("btop")["hint"].toString().contains("image-based"),"no system packages on an image-based host");

    UiApps mac(true,shell,nullptr,marker);check(mac);
    require(mac.entry("btop")["install"].toString().isEmpty() && mac.note().contains("Homebrew"),"Mac needs Homebrew");
    executable(bin+"/brew","#!/bin/sh\nexit 0\n");check(mac);
    require(mac.entry("btop")["install"]=="brew install btop" && mac.note().isEmpty(),"Mac installs with Homebrew");

    UiApps missing(false,root.filePath("no-such-shell"),nullptr,marker);check(missing);
    require(missing.entry("lazygit")["installed"].toInt()==1,"falls back to this process's PATH");
    std::puts("PASS: catalog merge and overrides, installed check on the login PATH, Homebrew first, system packages, image-based hosts, Mac, quoting, missing shell");
}
