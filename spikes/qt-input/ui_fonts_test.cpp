#include "ui_fonts.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
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
    std::puts("PASS: shared/platform font defaults, managed symlink preservation, preview/apply/cancel/reset, bounds and transactional invalid-file fallback");
}
