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
    // The shipped file's shape: a duplicate of a curated app, one per platform.
    const QString file=root.filePath("apps-catalog.json");
    executable(file,R"({"apps":[{"id":"lazygit","name":"dup","command":"lazygit","brew":"lazygit","mac":true,"linux":true},
        {"id":"glow","name":"Glow","command":"glow","description":"Render markdown","section":"Productivity","brew":"glow","mac":true,"linux":true},
        {"id":"macmon","name":"macmon","command":"macmon","brew":"macmon","mac":true,"linux":false},
        {"id":"odd","name":"Odd","command":"$X","brew":"odd"}]})");
    const auto curatedCount=UiApps::catalog(false,root.filePath("none")).size();
    require(UiApps::catalog(false,file).size()==curatedCount+1 && UiApps::catalog(true,file).size()==curatedCount+2,"shipped catalog per platform, curated first");
    require(UiApps::catalog(false,file)[0].featured && !UiApps::catalog(false,file).last().featured && UiApps::catalog(false,file).last().id=="glow","shipped apps after curated");

    require(UiApps::program("btop --utf-force")=="btop" && UiApps::program("FOO=1 btop").isEmpty() && UiApps::program("$HOME/x").isEmpty(),"program word");
    require(UiApps::program("~/bin/tool")==QDir::home().filePath("bin/tool"),"home program");
    require(UiApps::probeScript({"it's"},marker).contains("'it'\\''s'"),"quoted program");

    // Homebrew installed in a fixture prefix the shell does not load.
    const QString offPath=root.filePath("linuxbrew/bin/brew"),sources=root.filePath("state/app-sources.json");
    require(QDir().mkpath(root.filePath("linuxbrew/bin")),"brew prefix");executable(offPath,"#!/bin/sh\nexit 0\n");
    UiApps onLinux(false,shell,nullptr,marker,file,sources,{root.filePath("missing/brew"),offPath});
    require(onLinux.entries().size()==curatedCount+1 && onLinux.entry("lazygit")["installed"].toInt()==-1,"unchecked catalog");
    onLinux.setDefinitions({QVariantMap{{"id","mine"},{"name","Mine"},{"command","mine --flag"},{"directory",""}},
        QVariantMap{{"id","lazygit"},{"name","LG"},{"command","lazygit -p ~"},{"directory","~"}}});
    check(onLinux);
    require(onLinux.entries().first().toMap()["id"]=="mine" && onLinux.entry("mine")["installed"].toInt()==1 && !onLinux.entry("mine")["builtin"].toBool(),"own definition first");
    require(onLinux.entry("lazygit")["name"]=="LG" && onLinux.entry("lazygit")["command"]=="lazygit -p ~" && onLinux.entry("lazygit")["installed"].toInt()==1,"definition overrides a catalog entry");
    require(onLinux.entry("lazygit")["description"].toString().contains("Git"),"catalog description kept");
    require(onLinux.entry("btop")["installed"].toInt()==0 && onLinux.entry("btop")["install"]=="sudo dnf install btop" && onLinux.entry("btop")["via"]=="dnf","system package");
    require(onLinux.entry("lazygit")["options"].toList().isEmpty() || onLinux.entry("lazygit")["installed"].toInt()==1,"lazygit has no Fedora package");
    require(onLinux.entry("yazi")["install"].toString().isEmpty() && onLinux.entry("yazi")["hint"].toString().contains("COPR lihaohong/yazi") && onLinux.entry("yazi")["hint"].toString().contains("turned off"),"third-party repositories start off");
    require(onLinux.entry("glow")["install"].toString().isEmpty() && onLinux.entry("glow")["needsHomebrew"].toBool() && onLinux.entry("glow")["hint"].toString().contains("does not load it")
        && onLinux.entry("glow")["section"]=="Productivity","shipped apps install with Homebrew only");
    require(onLinux.homebrew()["path"]==offPath && onLinux.homebrew()["shellenv"]=="eval \"$("+offPath+" shellenv)\"" && !onLinux.homebrew()["found"].toBool() && onLinux.homebrew()["install"].toString().contains("Homebrew/install"),"Homebrew installed but not loaded");
    const auto sourceIds=[&](UiApps &apps){QStringList ids;for(const auto &value:apps.sources())ids<<value.toMap()["id"].toString();return ids;};
    require(sourceIds(onLinux)==QStringList({"brew","dnf","copr"}),"sources found here");
    onLinux.setSourceEnabled("copr",true);
    require(onLinux.entry("yazi")["install"]=="sudo dnf copr enable lihaohong/yazi && sudo dnf install yazi" && onLinux.entry("yazi")["via"]=="COPR lihaohong/yazi","a turned-on repository is offered");
    {QFile saved(sources);require(saved.open(QIODevice::ReadOnly) && saved.readAll().contains("\"copr\": true"),"source choices saved");}

    onLinux.setDefinitions({QVariantMap{{"id","later"},{"name","Later"},{"command","later"},{"directory",""}}});
    require(onLinux.entry("later")["installed"].toInt()==-1,"unknown until checked, not missing");
    executable(bin+"/brew","#!/bin/sh\nexit 0\n");check(onLinux);
    require(onLinux.entry("btop")["install"]=="brew install btop" && onLinux.entry("yazi")["install"]=="brew install yazi" && onLinux.entry("glow")["install"]=="brew install glow","Homebrew first on Linux");
    require(onLinux.entry("btop")["options"].toList().size()==2 && onLinux.entry("yazi")["options"].toList().size()==2,"every source offered");
    onLinux.setSourceEnabled("brew",false);
    require(onLinux.entry("btop")["install"]=="sudo dnf install btop" && onLinux.entry("glow")["hint"].toString().contains("Homebrew") && onLinux.entry("glow")["hint"].toString().contains("turned off"),"a turned-off source is not offered");
    onLinux.setSourceEnabled("brew",true);
    QFile::remove(bin+"/brew");executable(marker,"");check(onLinux);
    require(onLinux.entry("btop")["install"].toString().isEmpty() && onLinux.entry("btop")["needsHomebrew"].toBool() && onLinux.entry("btop")["hint"].toString().contains("Homebrew")
        && !onLinux.sources()[1].toMap()["found"].toBool() && onLinux.sources()[1].toMap()["detail"].toString().contains("Image-based") && onLinux.sources().size()==2,"no system packages on an image-based host");

    // Your own recipe comes first and setup follows any recipe; a definition
    // without install has no way to install from Band.
    onLinux.setDefinitions({QVariantMap{{"id","later"},{"name","Later"},{"command","later"},{"directory",""},{"install","cargo install later"},{"setup","later --init"}},
        QVariantMap{{"id","bare"},{"name","Bare"},{"command","bare"},{"directory",""}},
        QVariantMap{{"id","btop"},{"name","btop"},{"command","btop"},{"directory",""},{"install","make install"},{"setup","btop --config"}}});
    check(onLinux);
    require(onLinux.entry("later")["install"]=="cargo install later && later --init" && onLinux.entry("later")["via"]=="your recipe" && onLinux.entry("later")["setup"]=="later --init","own recipe with setup");
    require(onLinux.entry("bare")["install"].toString().isEmpty() && onLinux.entry("bare")["hint"].toString().contains("without install"),"a definition without install");
    const auto btopOptions=onLinux.entry("btop")["options"].toList();
    // (This host is image-based without brew by now, so only the own recipe is left.)
    require(btopOptions.size()==1 && btopOptions[0].toMap()["install"]=="make install && btop --config" && !onLinux.entry("btop")["needsHomebrew"].toBool(),"own recipe for a catalog app, with setup");
    UiApps mac(true,shell,nullptr,marker,file,{},{});check(mac);
    require(sourceIds(mac)==QStringList({"brew"}),"Mac sources");
    require(!mac.entry("macmon").isEmpty() && onLinux.entry("macmon").isEmpty(),"platform-only apps");
    require(mac.entry("btop")["install"].toString().isEmpty() && mac.note().contains("Homebrew"),"Mac needs Homebrew");
    executable(bin+"/brew","#!/bin/sh\nexit 0\n");check(mac);
    require(mac.entry("btop")["install"]=="brew install btop" && mac.note().isEmpty(),"Mac installs with Homebrew");

    UiApps missing(false,root.filePath("no-such-shell"),nullptr,marker,file,{},{});check(missing);
    require(missing.entry("lazygit")["installed"].toInt()==1,"falls back to this process's PATH");
    std::puts("PASS: curated and shipped catalog, merge and overrides, installed check on the login PATH, Homebrew first, every source offered, own install recipe and setup, sources on and off and saved, COPR off by default, Homebrew not loaded, system packages, image-based hosts, Mac, quoting, missing shell");
}
