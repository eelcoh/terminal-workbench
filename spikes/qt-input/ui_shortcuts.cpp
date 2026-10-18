#include "ui_shortcuts.h"
#include "shortcut_bridge.h"
#include <QFile>
#include <QKeySequence>
#include <QHash>
namespace {
struct Action {const char *source,*client;};
const Action actions[]={
    {"commands","commands"},{"overview","overview"},{"appearance","appearance"},
    {"terminal.new","new-terminal"},{"workspace.new","new-workspace"},
    {"terminal.previous","previous-terminal"},{"terminal.next","next-terminal"},
    {"workspace.previous","previous-workspace"},{"workspace.next","next-workspace"},
    {"workspace.rename","rename-workspace"},{"workspace.folder","workspace-folder"},
    {"terminal.stop","close-terminal"},{"terminal.width.cycle","cycle-width"},{"terminal.width.full","full-width"},
    {"shortcuts.help","help"},{"terminal.font.settings","font-settings"},
    {"terminal.font.increase","font-increase"},{"terminal.font.decrease","font-decrease"},{"terminal.font.reset","font-reset"}
};
int qtKey(int key){
    if((key>='A' && key<='Z') || (key>='0' && key<='9') || key==32 || key==45 || key==47 || key==61)return key;
    if(key>=290 && key<=300)return Qt::Key_F1+key-290;
    switch(key){case 257:return Qt::Key_Return;case 258:return Qt::Key_Tab;case 259:return Qt::Key_Backspace;
    case 260:return Qt::Key_Insert;case 261:return Qt::Key_Delete;case 262:return Qt::Key_Right;case 263:return Qt::Key_Left;
    case 264:return Qt::Key_Down;case 265:return Qt::Key_Up;case 266:return Qt::Key_PageUp;case 267:return Qt::Key_PageDown;
    case 268:return Qt::Key_Home;case 269:return Qt::Key_End;default:return Qt::Key_unknown;}
}
QKeySequence sequence(WorkbenchShortcut chord,bool apple){
    if(!chord.key)return {};
    Qt::KeyboardModifiers mods;
    if(chord.mods&1)mods|=apple?Qt::MetaModifier:Qt::ControlModifier;
    if(chord.mods&2)mods|=Qt::ShiftModifier;
    if(chord.mods&4)mods|=Qt::AltModifier;
    if(chord.mods&8)mods|=apple?Qt::ControlModifier:Qt::MetaModifier;
    return QKeySequence(QKeyCombination(mods,static_cast<Qt::Key>(qtKey(chord.key))));
}
QString label(WorkbenchShortcut chord,bool apple){
    if(!chord.key)return {};
    QString result;
    if(chord.mods&1)result+=apple?QStringLiteral("⌃"):QStringLiteral("Ctrl+");
    if(chord.mods&8)result+=apple?QStringLiteral("⌘"):QStringLiteral("Super+");
    if(chord.mods&4)result+=apple?QStringLiteral("⌥"):QStringLiteral("Alt+");
    if(chord.mods&2)result+=apple?QStringLiteral("⇧"):QStringLiteral("Shift+");
    return result+QKeySequence(qtKey(chord.key)).toString(QKeySequence::PortableText);
}
}
UiShortcuts::UiShortcuts(const QString &path,bool apple,QObject *parent):QObject(parent){
    WorkbenchShortcut chords[64];char error[256]={0};const auto encoded=QFile::encodeName(path);
    int count=workbench_shortcuts_read(path.isEmpty()?nullptr:encoded.constData(),apple,chords,64,error,sizeof(error));
    const auto fallback=[&]{count=workbench_shortcuts_read(nullptr,apple,chords,64,error,sizeof(error));};
    if(count<0){error_="Using default shortcuts. "+QString::fromUtf8(error);fallback();}
    QHash<QString,WorkbenchShortcut> source;
    const auto index=[&]{source.clear();for(int i=0;i<count;++i)source.insert(QString::fromLatin1(chords[i].id),chords[i]);};index();
    // Clipboard remains native in this slice. A remapped app chord must not steal it.
    for(const auto &action:actions){const auto chord=source.value(QString::fromLatin1(action.source));
        if((chord.key=='C' || chord.key=='V') && chord.mods==(apple?8u:3u)){
            error_="Using default shortcuts. Application binding conflicts with native copy/paste.";fallback();index();break;
        }
    }
    for(const auto &action:actions){const auto chord=source.value(QString::fromLatin1(action.source));const auto id=QString::fromLatin1(action.client);
        sequences_[id]=sequence(chord,apple).toString(QKeySequence::PortableText);labels_[id]=label(chord,apple);
    }
    // Qt-only move defaults yield to explicit managed actions on the same chord.
    const WorkbenchShortcut moves[]={{"move-left",263,14},{"move-right",262,14},{"move-up",265,14},{"move-down",264,14}};
    for(const auto &move:moves){const auto id=QString::fromLatin1(move.id),key=sequence(move,apple).toString(QKeySequence::PortableText);
        bool used=false;for(const auto &value:sequences_)if(value.toString()==key){used=true;break;}
        sequences_[id]=used?QString():key;labels_[id]=used?QString():label(move,apple);
    }
}
