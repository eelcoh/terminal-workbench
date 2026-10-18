#include "ui_fonts.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char *message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void write(const QString &path,const QByteArray &bytes){QFile file(path);require(file.open(QIODevice::WriteOnly),"fixture open");require(file.write(bytes)==bytes.size(),"fixture write");}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir dir;require(dir.isValid(),"private fixture");
    const auto target=dir.filePath("managed.conf"),link=dir.filePath("config.conf");
    const QByteArray managed="workbench-config-v1\n# preserve me\n[macos.font]\nsize = 20\n[font]\nfamily = Fira Code\nsize = 14\n[linux.font]\nsize = 16\n";
    write(target,managed);require(QFile::link(target,link),"managed symlink");
    UiFonts linuxFonts(link,false),macFonts(link,true);
    require(linuxFonts.error().isEmpty() && linuxFonts.current()["size"]==16 && macFonts.current()["size"]==20,"platform precedence");
    require(linuxFonts.current()["family"]=="Fira Code","shared family");
    linuxFonts.begin();require(linuxFonts.preview("JetBrains Mono",24),"valid live preview");linuxFonts.cancel();require(linuxFonts.current()==linuxFonts.defaults(),"cancel restores opening values");
    linuxFonts.begin();linuxFonts.preview("JetBrains Mono",24);linuxFonts.apply();linuxFonts.cancel();require(linuxFonts.current()["size"]==24,"apply retains window choice");
    linuxFonts.begin();linuxFonts.useDefaults();linuxFonts.cancel();require(linuxFonts.current()["family"]=="JetBrains Mono" && linuxFonts.current()["size"]==24,"defaults preview is cancellable");
    linuxFonts.resetSize();require(linuxFonts.current()["size"]==16 && linuxFonts.current()["family"]=="JetBrains Mono","size reset retains face");
    const auto before=linuxFonts.current();require(!linuxFonts.preview("unavailable",18) && !linuxFonts.preview("Fira Code",9) && !linuxFonts.preview("Fira Code",33) && linuxFonts.current()==before,"invalid preview transaction");
    for(int i=0;i<50;++i){linuxFonts.step(1);}
    require(linuxFonts.current()["size"]==32,"upper bound");
    for(int i=0;i<50;++i){linuxFonts.step(-1);}
    require(linuxFonts.current()["size"]==10,"lower bound");
    QFile check(target);require(check.open(QIODevice::ReadOnly) && check.readAll()==managed && QFileInfo(link).isSymLink(),"UI preserves managed bytes and symlink");check.close();
    write(target,"workbench-config-v1\n[font]\nfamily = Fira Code\nsize = 99\n");UiFonts invalid(link,false);
    require(!invalid.error().isEmpty() && invalid.current()["family"]=="JetBrains Mono" && invalid.current()["size"]==16,"invalid file falls back as one transaction");
    write(target,"workbench-config-v1\n[font]\nsize = 22\n[shortcuts]\ncopy = Ctrl+C\n");UiFonts invalidOther(link,false);require(!invalidOther.error().isEmpty() && invalidOther.current()["size"]==16,"other-section validation retained");
    write(target,"workbench-config-v1\n[appearance]\napplication-theme = absent-theme\nterminal-theme = also-absent\n[font]\nfamily = Fira Code\nsize = 19\n[macos.font]\nsize = 21\n");
    UiFonts missingThemeLinux(link,false),missingThemeMac(link,true);
    require(missingThemeLinux.error().isEmpty() && missingThemeLinux.current()["family"]=="Fira Code" && missingThemeLinux.current()["size"]==19,"unused missing theme must not discard Linux fonts");
    require(missingThemeMac.error().isEmpty() && missingThemeMac.current()["family"]=="Fira Code" && missingThemeMac.current()["size"]==21,"unused missing theme must not discard Mac fonts");
    write(dir.filePath("absent-theme"),"background = #invalid\n");UiFonts corruptTheme(link,true);
    require(corruptTheme.error().isEmpty() && corruptTheme.current()==missingThemeMac.current(),"unused malformed theme must not discard fonts");
    write(target,"workbench-config-v1\n[appearance]\nterminal-theme = absent\n[font]\nsize = 99\n");UiFonts badFontWithTheme(link,true);
    require(!badFontWithTheme.error().isEmpty() && badFontWithTheme.current()["size"]==16,"font validation remains active with unused themes");
    UiFonts absent(dir.filePath("missing"),true);require(absent.error().isEmpty() && absent.current()==invalid.defaults(),"missing config defaults");
    write(target,managed);const auto layer=dir.filePath("ui/linux/terminal-font.json");
    UiFonts saved(link,false,nullptr,layer);saved.begin();saved.preview("JetBrains Mono",18);saved.cancel();
    require(!QFile::exists(layer) && saved.current()==saved.defaults(),"preview cancel creates no file");
    saved.begin();saved.preview("JetBrains Mono",18);require(saved.apply(),"font durable apply");saved.cancel();
    UiFonts restored(link,false,nullptr,layer);require(restored.current()==saved.current() && restored.current()["size"]==18,"font survives reopening");
    require(!(QFile::permissions(layer)&(QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ReadOther|QFileDevice::WriteOther)),"private UI permissions");
    QFile savedFile(layer);require(savedFile.open(QIODevice::ReadOnly),"font store read");const auto savedBytes=savedFile.readAll();savedFile.close();
    restored.step(1);UiFonts afterShortcut(link,false,nullptr,layer);require(afterShortcut.current()["size"]==18,"size shortcuts are temporary until Apply");
    restored.begin();restored.useDefaults();require(restored.apply(),"save inherited font");
    require(savedFile.open(QIODevice::ReadOnly),"read reset layer");const auto sparse=QJsonDocument::fromJson(savedFile.readAll()).object();savedFile.close();require(sparse.size()==1 && sparse["version"].toInt()==1,"managed fields omitted");
    UiFonts stale(link,false,nullptr,layer);stale.begin();stale.preview("JetBrains Mono",22);write(layer,savedBytes);
    require(!stale.apply() && !stale.editError().isEmpty(),"stale UI settings reject save");stale.cancel();require(stale.current()==stale.defaults(),"failed save remains cancellable");
    UiFonts staleManaged(link,false,nullptr,layer);write(target,managed+"# external edit\n");staleManaged.begin();require(!staleManaged.apply(),"managed changes before opening editor rejected");write(target,managed);
    UiFonts locked(link,false,nullptr,layer);locked.begin();locked.preview("Fira Code",22);QLockFile lock(layer+".lock");require(lock.tryLock(0),"test lock");require(!locked.apply(),"locked writer rejected");lock.unlock();require(locked.apply(),"save after lock release");
    write(dir.filePath("blocked"),"file");UiFonts failed(link,false,nullptr,dir.filePath("blocked/font.json"));failed.begin();failed.preview("Fira Code",22);require(!failed.apply(),"write failure rejects Apply");failed.cancel();require(failed.current()==failed.defaults(),"write failure cancel restores current");
    const QList<QByteArray> bad={"{broken", "{\"version\":2}", "{\"version\":1,\"size\":18.5}", "{\"version\":1,\"family\":\"missing\"}", "{\"version\":1,\"size\":99}", "{\"version\":1,\"extra\":true}", QByteArray(4097,' ')};
    for(const auto &payload:bad){write(layer,payload);UiFonts invalidSaved(link,false,nullptr,layer);require(!invalidSaved.error().isEmpty() && invalidSaved.current()==invalidSaved.defaults(),"invalid saved font falls back transactionally");invalidSaved.begin();require(!invalidSaved.apply(),"invalid saved file preserved");require(savedFile.open(QIODevice::ReadOnly) && savedFile.readAll()==payload,"invalid bytes untouched");savedFile.close();}
    write(layer,"{\"version\":1,\"size\":19}");UiFonts sparseSaved(link,false,nullptr,layer);require(sparseSaved.current()["family"]=="Fira Code" && sparseSaved.current()["size"]==19,"omitted family inherits managed default");
    UiFonts macSaved(link,true,nullptr,dir.filePath("ui/macos/terminal-font.json"));require(macSaved.current()["size"]==20,"Mac separate platform layer");
    require(check.open(QIODevice::ReadOnly) && check.readAll()==managed && QFileInfo(link).isSymLink(),"persistent apply preserves managed symlink");check.close();
    std::puts("PASS: font defaults, preview/apply/cancel, sparse durable settings, restart, inherited defaults, managed symlinks, invalid stores, locks and failed/stale saves");
}
