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
    // Workspace defaults participate in the same transaction, including files
    // outside the config directory. Legacy global chrome fields are unused.
    QDir().mkpath(root.filePath("settings"));
    const auto appearancePath=root.filePath("settings/appearance.conf"),themePath=root.filePath("theme.default");
    const auto appearanceConfig=[](const char *theme,int size=17){return QByteArray("workbench-config-v1\n[appearance]\napplication-theme = missing-unused-app\nfollow-workspace = false\nterminal-theme = ")+theme+"\n[font]\nsize = "+QByteArray::number(size)+"\n";};
    write(appearancePath,appearanceConfig("Workbench Dawn"));
    UiThemes themes("");UiAppearance appearance(appearancePath,false,themes);
    UiFonts appearanceFonts(appearancePath,false);UiShortcuts appearanceKeys(appearancePath,false);
    UiConfiguration colors(appearanceFonts,appearanceKeys,true,nullptr,&appearance);
    require(colors.error().isEmpty() && colors.themeIndex()==1,"managed builtin workspace default, unused application fields ignored");
    write(appearancePath,appearanceConfig("../theme.default",19));
    require(wait([&]{return !colors.error().isEmpty();}) && colors.themeIndex()==1 && size(appearanceFonts)==17,"missing selected theme rejects font and appearance transaction");
    write(themePath,"background = #123456\nforeground = #ffffff\ncommand = never-run\n");
    require(wait([&]{return colors.error().isEmpty() && size(appearanceFonts)==19;}),"creating missing selected theme recovers automatically");
    const int initialIndex=colors.themeIndex(),registered=workbench_theme_count();
    require(themes.theme(initialIndex)["background"]=="#123456","custom .default palette selected");
    for(int i=0;i<10;++i)require(colors.reload(),"unchanged appearance reload succeeds");
    require(workbench_theme_count()==registered && colors.themeIndex()==initialIndex,"unchanged reload reuses immutable theme slot");
    colors.setEditing(true);write(themePath,"background = #654321\nforeground = #ffffff\n");
    QElapsedTimer deferred;deferred.start();while(deferred.elapsed()<300){QCoreApplication::processEvents();QThread::msleep(10);}
    require(colors.themeIndex()==initialIndex,"appearance preview draft defers file edits");colors.setEditing(false);
    require(wait([&]{return themes.theme(colors.themeIndex())["background"]=="#654321";}),"referenced theme atomic replacement reloads without managed edit");
    WorkbenchTheme old;require(workbench_theme_get(initialIndex,&old) && old.background.r==0x12,"previous color snapshot remains immutable");
    write(themePath,"background = invalid\n");
    require(wait([&]{return !colors.error().isEmpty();}) && themes.theme(colors.themeIndex())["background"]=="#654321","invalid color file keeps last valid theme");
    QFile::remove(themePath);require(!colors.reload() && themes.theme(colors.themeIndex())["background"]=="#654321","deleted selected theme keeps colors");
    const auto remoteDirectory=root.filePath("external");QDir().mkpath(remoteDirectory);const auto remote=remoteDirectory+"/palette.default";
    require(QFile::link(remote,themePath),"dangling theme symlink fixture");colors.reload();
    write(remote,"background = #fedcba\nforeground = #000000\n");
    require(wait([&]{return colors.error().isEmpty() && themes.theme(colors.themeIndex())["background"]=="#fedcba";}),"dangling theme symlink target creation repairs reload");
    write(remote,"background = #abcdef\nforeground = #000000\n");
    require(wait([&]{return themes.theme(colors.themeIndex())["background"]=="#abcdef";}),"external symlink target remains watched");
    write(appearancePath,appearanceConfig("Workbench Night"));
    require(wait([&]{return colors.error().isEmpty() && colors.themeIndex()==0;}),"return to builtin drops missing file dependency");
    write(appearancePath,"workbench-config-v1\n[macos.appearance]\nterminal-theme = Dracula\n[linux.appearance]\nterminal-theme = Workbench Dawn\n[appearance]\nterminal-theme = Workbench Night\n");
    UiAppearance mac(appearancePath,true,themes),linuxAppearance(appearancePath,false,themes);
    require(mac.themeIndex()==2 && linuxAppearance.themeIndex()==1,"platform appearance override wins regardless of section order");
    // Creation preferences never probe project folders during reload; the
    // existing session-service Create boundary checks availability explicitly.
    const auto creationPath=root.filePath("settings/creation.conf");
    write(creationPath,"workbench-config-v1\n[workspace]\ndirectory = ../Project café\nterminal-width = 1/3\n[macos.workspace]\nterminal-width = 2/3\n");
    const auto shared=CreationDefaults::read(creationPath,false),appleDefaults=CreationDefaults::read(creationPath,true);
    require(shared.error.isEmpty() && shared.directory==root.filePath("Project café") && shared.terminalWidth==1.0/3.0,"relative Unicode project folder resolved beside managed config");
    require(appleDefaults.error.isEmpty() && appleDefaults.directory==shared.directory && appleDefaults.terminalWidth==2.0/3.0,"platform width inherits shared directory independently");
    UiFonts creationFonts(creationPath,false);UiShortcuts creationKeys(creationPath,false);
    UiConfiguration creationSettings(creationFonts,creationKeys);
    write(creationPath,"workbench-config-v1\n[workspace]\ndirectory = ~/Workbench projects\nterminal-width = 2/3\n[font]\nsize = 21\n");
    require(wait([&]{return creationSettings.terminalWidth()==2.0/3.0 && size(creationFonts)==21;}),"creation and font defaults follow managed changes");
    require(creationSettings.workspaceDirectory()==QDir::home().filePath("Workbench projects"),"home-relative folder expanded without requiring directory availability");
    const auto oldDirectory=creationSettings.workspaceDirectory();
    write(creationPath,"workbench-config-v1\n[workspace]\nterminal-width = 0.4\n[font]\nsize = 22\n");
    require(wait([&]{return !creationSettings.error().isEmpty();}) && size(creationFonts)==21 && creationSettings.terminalWidth()==2.0/3.0 && creationSettings.workspaceDirectory()==oldDirectory,"invalid creation preference retains entire live transaction");
    creationSettings.setEditing(true);write(creationPath,"workbench-config-v1\n[workspace]\ndirectory = ~\nterminal-width = 1/2\n");
    require(!creationSettings.reload() && creationSettings.workspaceDirectory()==oldDirectory,"creation editor defers external defaults");
    creationSettings.setEditing(false);require(wait([&]{return creationSettings.error().isEmpty() && creationSettings.workspaceDirectory()==QDir::homePath();}),"deferred creation defaults accepted after dialog closes");
    QFile::remove(creationPath);require(wait([&]{return creationSettings.workspaceDirectory().isEmpty() && creationSettings.terminalWidth()==0.5;}),"managed file removal restores inherited folder and half width");
    for(const auto *width:{"1/1","3/3"}){
        write(creationPath,QByteArray("workbench-config-v1\n[workspace]\nterminal-width = ")+width+"\n");
        require(creationSettings.reload() && creationSettings.terminalWidth()==1.0,"full-width aliases survive managed reload");
    }
    std::puts("PASS: creation folders/widths, Unicode/home paths, platform field precedence, transaction rollback, deferred drafts,  managed workspace themes, watched Ghostty files, platform precedence, immutable colors, atomic font/shortcut reload, invalid last-valid retention, independent zoom, peer updates, editor deferral, override deletion and symlink replacement");
}
