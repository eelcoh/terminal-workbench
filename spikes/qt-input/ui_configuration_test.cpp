#include "ui_configuration.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <functional>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>

static void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void write(const QString &path,const QByteArray &data){QSaveFile file(path);require(file.open(QIODevice::WriteOnly) && file.write(data)==data.size() && file.commit(),"atomic fixture write");}
static QByteArray managed(int size,const char *chord="F10") {return QByteArray("workbench-config-v1\n# preserve managed comments\n[appearance]\nterminal-theme = missing-unused-theme\n[font]\nfamily = Fira Code\nsize = ")+QByteArray::number(size)+"\n[shortcuts]\ncommands = "+chord+"\n";}
static bool wait(const std::function<bool()> &predicate){QElapsedTimer time;time.start();while(time.elapsed()<4000){QCoreApplication::processEvents();if(predicate())return true;QThread::msleep(10);}return false;}
static int size(const UiFonts &fonts){return fonts.current()["size"].toInt();}
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);QTemporaryDir root;require(root.isValid(),"private root");
    const auto target=root.filePath("managed.conf"),path=root.filePath("config.conf"),font=root.filePath("ui/font.json"),keys=root.filePath("ui/shortcuts.conf");
    write(target,managed(16));require(QFile::link(target,path),"managed symlink fixture");
    UiFonts fonts(path,false,nullptr,font);UiShortcuts shortcuts(path,false,nullptr,keys);UiConfiguration settings(fonts,shortcuts,false);
    require(fonts.error().isEmpty() && shortcuts.error().isEmpty() && size(fonts)==16,"startup ignores unused appearance files");
    bool atomic=true;QObject::connect(&fonts,&UiFonts::changed,&app,[&]{if(size(fonts)==18)atomic=shortcuts.sequences()["commands"]=="F11";});
    write(target,managed(18,"F11"));require(settings.reload() && size(fonts)==18 && atomic,"both domains installed before notification");
    require(QFileInfo(path).isSymLink(),"managed symlink preserved");
    QDir().mkpath(QFileInfo(font).absolutePath());write(font,"{\"version\":1,\"size\":99}");write(target,managed(20,"F8"));
    const auto oldKeys=shortcuts.sequences();require(!settings.reload() && size(fonts)==18 && shortcuts.sequences()==oldKeys && !settings.error().isEmpty(),"invalid override rejects entire candidate");
    QFile::remove(font);require(settings.reload() && size(fonts)==20 && settings.error().isEmpty(),"repaired files recover without restart");
    fonts.step(1);require(size(fonts)==21,"temporary zoom");write(target,managed(20,"F7"));require(settings.reload() && size(fonts)==21,"shortcut-only changes preserve local temporary zoom");
    require(QFileInfo(path).isSymLink(),"no managed rewrite");
    // Event-driven peer updates, atomic file replacement,
    // open-editor deferral, deletion and symlink retargeting.
    UiFonts firstFonts(path,false,nullptr,font),peerFonts(path,false,nullptr,font);
    UiShortcuts firstKeys(path,false,nullptr,keys),peerKeys(path,false,nullptr,keys);
    UiConfiguration first(firstFonts,firstKeys),peer(peerFonts,peerKeys);
    first.setEditing(true);firstFonts.begin();firstFonts.preview("JetBrains Mono",23);
    write(target,managed(22,"F6"));require(wait([&]{return size(peerFonts)==22;}),"peer follows atomic managed-file replacement");
    require(size(firstFonts)==23,"open editor draft preserved");firstFonts.cancel();first.setEditing(false);
    require(wait([&]{return size(firstFonts)==22;}),"deferred update after editor closes");
    write(font,"{\"version\":1,\"size\":24}");require(wait([&]{return size(firstFonts)==24 && size(peerFonts)==24;}),"UI override propagates to peers");
    write(keys,"not a valid shortcut layer\n");write(font,"{\"version\":1,\"size\":25}");
    require(wait([&]{return !peer.error().isEmpty();}) && size(peerFonts)==24 && size(firstFonts)==24,"malformed peer settings retain last valid maps");
    QFile::remove(keys);QFile::remove(font);require(wait([&]{return peer.error().isEmpty() && size(firstFonts)==22 && size(peerFonts)==22;}),"override deletion returns to managed values");
    const auto replacement=root.filePath("replacement.conf");write(replacement,managed(26,"F5"));require(QFile::remove(path) && QFile::link(replacement,path),"replace managed symlink");
    require(wait([&]{return size(firstFonts)==26 && size(peerFonts)==26;}),"symlink retarget followed");
    write(replacement,managed(27,"F4"));require(wait([&]{return size(peerFonts)==27;}),"retargeted file remains watched");
    std::puts("PASS: atomic font/shortcut reload, invalid last-valid retention, independent zoom, peer updates, editor deferral, override deletion and symlink replacement");
}
