#include "ui_workspace_appearance.h"
#include "ui_themes.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QElapsedTimer>
#include <QThread>
#include <QDir>
#include <functional>
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char *message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static QByteArray bytes(const QString &path){QFile file(path);check(file.open(QIODevice::ReadOnly),"read fixture");return file.readAll();}
static void write(const QString &path,const QByteArray &text){QSaveFile file(path);check(file.open(QIODevice::WriteOnly) && file.write(text)==text.size() && file.commit(),"atomic fixture write");}
static bool wait(const std::function<bool()> &ready){QElapsedTimer time;time.start();while(time.elapsed()<4000){QCoreApplication::processEvents();if(ready())return true;QThread::msleep(10);}return false;}
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);QTemporaryDir root;check(root.isValid(),"private root");
    const auto directory=root.filePath("workspace-appearance"),firstPath=directory+"/w1.json";
    UiThemes themes("");const auto dark=themes.snapshot(0),light=themes.snapshot(1);
    UiWorkspaceAppearance first(directory),peer(directory);first.setWorkspaces({"w1","w2"});peer.setWorkspaces({"w1","w2"});
    check(!QFileInfo::exists(directory) && first.choice("w1").isEmpty(),"startup leaves legacy views and filesystem untouched");
    check(!first.begin("../escape") && !QFileInfo::exists(root.filePath("escape.json")),"invalid identity cannot address outside preference directory");
    check(first.begin("w1") && first.apply("w1",false,dark),"explicit workspace palette apply");first.end();
    check(wait([&]{return peer.choice("w1")["theme"].toMap()==dark;}),"peer follows initial directory creation and atomic file write");
    check(!(QFileInfo(firstPath).permissions()&(QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ExeGroup|QFileDevice::ReadOther|QFileDevice::WriteOther|QFileDevice::ExeOther)),"private saved palette permissions");
    const auto darkBytes=bytes(firstPath);check(peer.begin("w1"),"open peer draft");
    check(first.begin("w1") && first.apply("w1",false,light),"owner updates edited workspace");first.end();
    check(peer.choice("w1")["theme"].toMap()==dark,"open peer draft baseline retained");
    check(!peer.apply("w1",false,dark) && peer.error().contains("changed in another window"),"same-workspace conflict rejected");
    const auto lightBytes=bytes(firstPath);check(lightBytes!=darkBytes,"owner choice persisted");peer.end();check(peer.choice("w1")["theme"].toMap()==light,"Cancel adopts peer change");
    check(first.begin("w1") && peer.begin("w2"),"independent drafts");
    check(first.apply("w1",false,dark) && peer.apply("w2",false,light),"different workspace saves merge independently");first.end();peer.end();
    check(wait([&]{return first.choice("w2")["theme"].toMap()==light && peer.choice("w1")["theme"].toMap()==dark;}),"both scopes observed");
    check(first.begin("w1") && first.apply("w1",true,{}),"explicit managed-default marker");first.end();
    check(wait([&]{return peer.choice("w1")["default"].toBool();}),"default inheritance propagates");
    UiWorkspaceAppearance restarted(directory,false);restarted.setWorkspaces({"w1","w2"});
    check(restarted.choice("w1")["default"].toBool() && restarted.choice("w2")["theme"].toMap()==light,"snapshot/default choices survive restart");
    write(firstPath,"{broken");check(wait([&]{return !peer.error().isEmpty();}) && peer.choice("w1")["default"].toBool(),"invalid live file preserves last valid scope");
    check(peer.begin("w1") && !peer.apply("w1",false,dark) && bytes(firstPath)=="{broken","invalid existing file is never overwritten");peer.end();
    check(first.begin("w2") && first.apply("w2",false,dark),"valid workspace remains independently editable");first.end();
    check(wait([&]{return peer.choice("w2")["theme"].toMap()==dark;}),"one broken scope does not block another");
    write(firstPath,lightBytes);check(wait([&]{return peer.error().isEmpty() && peer.choice("w1")["theme"].toMap()==light;}),"repaired file recovers automatically");
    auto invalid=light;invalid["colors"]="background = invalid\n";
    check(peer.begin("w1") && !peer.apply("w1",false,invalid) && bytes(firstPath)==lightBytes,"invalid input does not touch saved palette");peer.end();
    QLockFile lock(firstPath+".lock");check(lock.tryLock(0),"held writer fixture");
    check(peer.begin("w1") && !peer.apply("w1",false,dark) && bytes(firstPath)==lightBytes,"concurrent writer lock preserves draft and file");lock.unlock();
    check(peer.apply("w1",false,dark),"writer can retry same draft after lock released");peer.end();
    write(firstPath,QByteArray(80*1024+1,' '));check(wait([&]{return !peer.error().isEmpty();}) && peer.choice("w1")["theme"].toMap()==dark,"oversized file keeps last valid colors");
    write(firstPath,lightBytes);check(wait([&]{return peer.error().isEmpty();}),"bounded repair");
    check(peer.begin("w1") && QFile::remove(firstPath),"external deletion fixture");
    check(!peer.apply("w1",false,dark) && !QFileInfo::exists(firstPath),"deleted preference conflicts with open editor");peer.end();check(peer.choice("w1").isEmpty(),"missing file leaves window snapshot fallback available");
    const auto blocked=root.filePath("blocked");write(blocked,"file");UiWorkspaceAppearance failed(blocked,false);failed.setWorkspaces({"w1"});
    check(failed.begin("w1") && !failed.apply("w1",false,dark) && bytes(blocked)=="file","failed write stays local and retains blocking file");
    auto choice=QVariantMap{{"version",1},{"default",false},{"theme",dark}};check(UiWorkspaceAppearance::validChoice(choice),"bounded snapshot schema");
    choice["extra"]=true;check(!UiWorkspaceAppearance::validChoice(choice),"unknown choice fields rejected");choice.remove("extra");choice["default"]="false";check(!UiWorkspaceAppearance::validChoice(choice),"default marker stays typed");
    first.setWorkspaces({});check(first.choice("w2").isEmpty() && QFileInfo::exists(directory+"/w2.json"),"catalog removal drops client entry without deleting saved metadata");
    std::puts("PASS: workspace-scoped atomic snapshots/defaults, peer notifications, restart, private files, independent saves, stale drafts, locks, invalid/oversized retention and repair, safe identities and local failures");
}
