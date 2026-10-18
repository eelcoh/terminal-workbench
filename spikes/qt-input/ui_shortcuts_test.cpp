#include "ui_shortcuts.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QKeySequence>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char *message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void write(const QString &path,const QByteArray &bytes){QFile file(path);require(file.open(QIODevice::WriteOnly),"fixture open");require(file.write(bytes)==bytes.size(),"fixture write");}
static bool chord(const UiShortcuts &keys,const char *id,const char *expected){return QKeySequence(keys.sequences().value(id).toString())==QKeySequence(QString::fromLatin1(expected));}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir dir;require(dir.isValid(),"private fixture");const auto path=dir.filePath("config.conf");
    UiShortcuts linuxDefaults(path,false),macDefaults(path,true);
    require(chord(linuxDefaults,"commands","Meta+Alt+P") && chord(macDefaults,"commands","Ctrl+Alt+P"),"matching application modifiers");
    require(macDefaults.labels()["commands"].toString()==QStringLiteral("⌘⌥P"),"Mac command option label");
    const QByteArray managed="workbench-config-v1\n# preserve me\n[macos.shortcuts]\ncommands = F9\n[shortcuts]\ncommands = F8\n[linux.shortcuts]\nshortcuts.help = F11\n[appearance]\nterminal-theme = unavailable-theme\n";
    write(path,managed);UiShortcuts linuxKeys(path,false),macKeys(path,true);
    require(linuxKeys.error().isEmpty() && macKeys.error().isEmpty() && chord(linuxKeys,"commands","F8") && chord(macKeys,"commands","F9") && chord(linuxKeys,"help","F11"),"shared/platform precedence ignores unused theme file");
    QFile check(path);require(check.open(QIODevice::ReadOnly) && check.readAll()==managed,"managed bytes unchanged");check.close();
    write(path,"workbench-config-v1\n[macos.shortcuts]\ncommands = Ctrl+Alt+P\n");UiShortcuts physicalControl(path,true);
    require(chord(physicalControl,"commands","Meta+Alt+P") && physicalControl.labels()["commands"].toString()==QStringLiteral("⌃⌥P"),"Mac physical Control differs from Command");
    write(path,"workbench-config-v1\n[shortcuts]\ncommands = none\nterminal.search = F8\nterminal.next = Super+Alt+Shift+Right\n");UiShortcuts unbound(path,false);
    require(unbound.error().isEmpty() && unbound.sequences()["commands"].toString().isEmpty() && !unbound.sequences().contains("terminal.search"),"unbound and unsupported actions stay unintercepted");
    require(chord(unbound,"next-terminal","Meta+Alt+Shift+Right") && unbound.sequences()["move-right"].toString().isEmpty() && !unbound.labels()["move-left"].toString().isEmpty(),"inherited move binding yields to configured action");
    write(path,"workbench-config-v1\n[shortcuts]\ncommands = F8\noverview = F8\n");UiShortcuts conflict(path,false);require(!conflict.error().isEmpty() && conflict.sequences()==linuxDefaults.sequences(),"duplicate fallback transaction");
    write(path,"workbench-config-v1\n[shortcuts]\ncommands = Ctrl+C\n");UiShortcuts reserved(path,false);require(!reserved.error().isEmpty() && reserved.sequences()==linuxDefaults.sequences(),"reserved shell controls remain reserved");
    write(path,"workbench-config-v1\n[shortcuts]\ncopy = none\nterminal.new = Ctrl+Shift+C\n");UiShortcuts clipboard(path,false);require(!clipboard.error().isEmpty() && clipboard.sequences()==linuxDefaults.sequences(),"native clipboard cannot be stolen");
    write(path,"workbench-config-v1\n[macos.shortcuts]\ncopy = none\nterminal.new = Command+C\n");UiShortcuts macClipboard(path,true);require(!macClipboard.error().isEmpty() && macClipboard.sequences()==macDefaults.sequences(),"native Mac clipboard cannot be stolen");
    std::puts("PASS: managed shortcut precedence, Mac modifier mapping, labels, unbinding, unsupported actions, move collisions, native clipboard reservation and transactional fallback");
}
