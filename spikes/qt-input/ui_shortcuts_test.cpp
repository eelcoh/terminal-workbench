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
    require(chord(linuxDefaults,"terminal-search","Meta+Alt+S") && chord(macDefaults,"terminal-search","Ctrl+Alt+S") && linuxDefaults.editable("terminal-search"),"search default modifiers and editable mapping");
    require(macDefaults.labels()["commands"].toString()==QStringLiteral("⌘⌥P"),"Mac command option label");
    const QByteArray managed="band-config-v1\n# preserve me\n[macos.shortcuts]\ncommands = F9\n[shortcuts]\ncommands = F8\n[linux.shortcuts]\nshortcuts.help = F11\n[appearance]\nterminal-theme = unavailable-theme\n";
    require(linuxDefaults.editable("next-waiting") && linuxDefaults.sequences()["next-waiting"]=="Meta+Alt+J","Linux attention chord is configurable");
    require(macDefaults.labels()["previous-waiting"]==QString::fromUtf8("⌘⌥K"),"Mac attention uses the same physical modifiers");
    write(path,"band-config-v1\n[shortcuts]\ncommands = Super+Alt+J\n");UiShortcuts attentionConflict(path,false);
    require(attentionConflict.error().isEmpty() && attentionConflict.sequences()["next-waiting"].toString().isEmpty() && chord(attentionConflict,"commands","Meta+Alt+J"),"new inherited attention chord yields to an existing custom binding");
    write(path,managed);UiShortcuts linuxKeys(path,false),macKeys(path,true);
    require(linuxKeys.error().isEmpty() && macKeys.error().isEmpty() && chord(linuxKeys,"commands","F8") && chord(macKeys,"commands","F9") && chord(linuxKeys,"help","F11"),"shared/platform precedence ignores unused theme file");
    QFile check(path);require(check.open(QIODevice::ReadOnly) && check.readAll()==managed,"managed bytes unchanged");check.close();
    write(path,"band-config-v1\n[macos.shortcuts]\ncommands = Ctrl+Alt+P\n");UiShortcuts physicalControl(path,true);
    require(chord(physicalControl,"commands","Meta+Alt+P") && physicalControl.labels()["commands"].toString()==QStringLiteral("⌃⌥P"),"Mac physical Control differs from Command");
    write(path,"band-config-v1\n[shortcuts]\ncommands = none\nterminal.search = F8\nterminal.next = Super+Alt+Shift+Right\n");UiShortcuts unbound(path,false);
    require(unbound.error().isEmpty() && unbound.sequences()["commands"].toString().isEmpty() && !unbound.sequences().contains("terminal.search"),"unbound actions and source IDs stay unintercepted");
    require(chord(unbound,"terminal-search","F8"),"managed terminal.search override reaches Qt search");
    require(chord(unbound,"next-terminal","Meta+Alt+Shift+Right") && unbound.sequences()["move-right"].toString().isEmpty() && !unbound.labels()["move-left"].toString().isEmpty(),"inherited move binding yields to configured action");
    write(path,"band-config-v1\n[shortcuts]\ncommands = F8\noverview = F8\n");UiShortcuts conflict(path,false);require(!conflict.error().isEmpty() && conflict.sequences()==linuxDefaults.sequences(),"duplicate fallback transaction");
    write(path,"band-config-v1\n[shortcuts]\ncommands = Ctrl+C\n");UiShortcuts reserved(path,false);require(!reserved.error().isEmpty() && reserved.sequences()==linuxDefaults.sequences(),"reserved shell controls remain reserved");
    write(path,"band-config-v1\n[shortcuts]\ncopy = none\nterminal.new = Ctrl+Shift+C\n");UiShortcuts clipboard(path,false);require(!clipboard.error().isEmpty() && clipboard.sequences()==linuxDefaults.sequences(),"native clipboard cannot be stolen");
    write(path,"band-config-v1\n[macos.shortcuts]\ncopy = none\nterminal.new = Command+C\n");UiShortcuts macClipboard(path,true);require(!macClipboard.error().isEmpty() && macClipboard.sequences()==macDefaults.sequences(),"native Mac clipboard cannot be stolen");
    write(path,managed);const auto layer=dir.filePath("ui/linux/shortcuts.conf");
    UiShortcuts editor(path,false,nullptr,layer);editor.begin();
    require(editor.record("commands",Qt::Key_F7,0),"record supported function key");
    require(chord(editor,"commands","F8"),"draft must not change live binding");
    require(!editor.record("overview",Qt::Key_F7,0) && !editor.editError().isEmpty(),"reject duplicate draft");
    require(!editor.record("commands",Qt::Key_C,Qt::ControlModifier),"reserve shell interrupt");
    require(!editor.record("commands",Qt::Key_V,Qt::ControlModifier|Qt::ShiftModifier),"reserve native paste");
    require(!editor.record("commands",Qt::Key_A,0),"reserve unmodified typing");
    require(editor.apply() && chord(editor,"commands","F7"),"save and activate draft");
    UiShortcuts reopened(path,false,nullptr,layer);require(chord(reopened,"commands","F7"),"persistent layer reload");
    QFile layerFile(layer);require(layerFile.open(QIODevice::ReadOnly),"read sparse layer");const auto layerBytes=layerFile.readAll();layerFile.close();
    require(layerBytes.contains("commands=F7") && !layerBytes.contains("terminal.new="),"sparse UI layer");
    reopened.begin();require(reopened.clear("commands") && reopened.apply(),"persistent unbind");
    UiShortcuts cleared(path,false,nullptr,layer);require(cleared.sequences()["commands"].toString().isEmpty(),"unbound survives restart");
    cleared.begin();require(cleared.reset("commands") && cleared.apply() && chord(cleared,"commands","F8"),"restore managed default");
    QFile managedFile(path);require(managedFile.open(QIODevice::ReadOnly) && managedFile.readAll()==managed,"editor preserves managed bytes");managedFile.close();
    UiShortcuts stale(path,false,nullptr,layer);stale.begin();require(stale.record("commands",Qt::Key_F6,0),"stale draft");
    write(layer,"band-shortcuts-v1\ncommands=F5\n");require(!stale.apply() && chord(stale,"commands","F8"),"reject concurrent UI edits");
    UiShortcuts staleManaged(path,false,nullptr,layer);write(path,managed+"# external edit\n");staleManaged.begin();
    require(!staleManaged.apply(),"detect managed edit before editor opens");
    write(path,managed);UiShortcuts macEditor(path,true,nullptr,dir.filePath("ui/macos/shortcuts.conf"));macEditor.begin();
    require(macEditor.record("commands",Qt::Key_L,Qt::ControlModifier|Qt::AltModifier) && macEditor.apply() && chord(macEditor,"commands","Ctrl+Alt+L"),"Mac recording uses physical Command");
    write(path,"band-config-v1\n[shortcuts]\nforward = F6\n");UiShortcuts forwarding(path,false);
    require(chord(forwarding,"forward","F6") && forwarding.editable("forward"),"forward action managed and editable");
    std::puts("PASS: managed/platform shortcuts, reserved keys, transactional drafts, sparse UI persistence, reset, unbind and stale-file protection");
}
