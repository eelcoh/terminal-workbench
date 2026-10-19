#include "ui_shortcuts.h"
#include "shortcut_bridge.h"
#include <QFile>
#include <QKeySequence>
#include <QHash>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
namespace {
struct Action {const char *source,*client;};
const Action actions[]={
    {"forward","forward"},{"shortcuts","shortcut-settings"},{"commands","commands"},{"overview","overview"},{"appearance","appearance"},
    {"terminal.search","terminal-search"},{"terminal.new","new-terminal"},{"workspace.new","new-workspace"},
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
namespace {
QByteArray bytes(const QString &path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.read(65537):QByteArray();}
int sourceKey(int key){
    if((key>='A' && key<='Z') || (key>='0' && key<='9') || key==32 || key==45 || key==47 || key==61)return key;
    if(key>=Qt::Key_F1 && key<=Qt::Key_F11)return 290+key-Qt::Key_F1;
    for(int candidate=257;candidate<=269;++candidate)if(qtKey(candidate)==key)return candidate;
    return -1;
}
}
UiShortcuts::UiShortcuts(const QString &path,bool apple,QObject *parent,const QString &overrides)
    :QObject(parent),path_(path),overrides_(overrides),apple_(apple){
    WorkbenchShortcut chords[64];char error[256]={0};const auto encoded=QFile::encodeName(path);
    int count=workbench_shortcuts_read(path.isEmpty()?nullptr:encoded.constData(),apple,chords,64,error,sizeof(error));
    if(count<0){error_="Using default shortcuts. "+QString::fromUtf8(error);count=workbench_shortcuts_read(nullptr,apple,chords,64,error,sizeof(error));}
    if(count>0)for(int i=0;i<count;++i)base_.append(chords[i]);
    QString diagnostic;
    if(!validate(base_,diagnostic)){
        error_="Using default shortcuts. "+diagnostic;
        count=workbench_shortcuts_read(nullptr,apple,chords,64,error,sizeof(error));base_.clear();for(int i=0;i<count;++i)base_.append(chords[i]);
    }
    current_=base_;
    if(!overrides_.isEmpty()){
        const auto encodedOverride=QFile::encodeName(overrides_);auto next=base_;
        if(workbench_shortcuts_layer(encodedOverride.constData(),apple_,next.data(),next.size(),error,sizeof(error))<0 || !validate(next,diagnostic))
            error_="Using managed shortcuts. "+(diagnostic.isEmpty()?QString::fromUtf8(error):diagnostic);
        else current_=next;
    }
    draft_=current_;managedBytes_=bytes(path_);overrideBytes_=bytes(overrides_);rebuild();
}
bool UiShortcuts::validate(const QVector<WorkbenchShortcut> &chords,QString &error) const {
    char message[256]={0};if(!workbench_shortcuts_valid(chords.constData(),chords.size(),message,sizeof(message))){error=QString::fromUtf8(message);return false;}
    for(const auto &action:actions)for(const auto &chord:chords)if(QString::fromLatin1(chord.id)==QString::fromLatin1(action.source)
        && (chord.key=='C' || chord.key=='V') && chord.mods==(apple_?8u:3u)){
        error="Application binding conflicts with native copy/paste.";return false;
    }
    return true;
}
int UiShortcuts::index(const QString &id) const {
    for(const auto &action:actions)if(id==QString::fromLatin1(action.client))
        for(int i=0;i<current_.size();++i)if(QString::fromLatin1(current_[i].id)==QString::fromLatin1(action.source))return i;
    return -1;
}
bool UiShortcuts::editable(const QString &id) const {return index(id)>=0;}
void UiShortcuts::rebuild(){
    sequences_.clear();labels_.clear();
    for(const auto &action:actions){const auto id=QString::fromLatin1(action.client);const auto chord=current_.value(index(id));
        sequences_[id]=sequence(chord,apple_).toString(QKeySequence::PortableText);labels_[id]=label(chord,apple_);
    }
    const WorkbenchShortcut moves[]={{"move-left",263,14},{"move-right",262,14},{"move-up",265,14},{"move-down",264,14}};
    for(const auto &move:moves){const auto id=QString::fromLatin1(move.id),key=sequence(move,apple_).toString(QKeySequence::PortableText);
        bool used=false;for(const auto &value:sequences_)if(value.toString()==key){used=true;break;}
        sequences_[id]=used?QString():key;labels_[id]=used?QString():label(move,apple_);
    }
}
QVariantMap UiShortcuts::draftLabels() const {
    QVariantMap result;for(const auto &action:actions){const auto id=QString::fromLatin1(action.client);result[id]=label(draft_.value(index(id)),apple_);}return result;
}
void UiShortcuts::begin(){draft_=current_;editError_.clear();Q_EMIT changed();}
bool UiShortcuts::set(const QString &id,int key,unsigned mods){
    const int i=index(id);if(i<0)return false;
    auto next=draft_;next[i].key=key;next[i].mods=mods;QString error;
    if(!validate(next,error)){editError_=error;Q_EMIT changed();return false;}
    draft_=next;editError_.clear();Q_EMIT changed();return true;
}
bool UiShortcuts::record(const QString &id,int key,int modifiers){
    unsigned mods=0;
    if(modifiers&Qt::ControlModifier)mods|=apple_?8u:1u;
    if(modifiers&Qt::MetaModifier)mods|=apple_?1u:8u;
    if(modifiers&Qt::ShiftModifier)mods|=2u;
    if(modifiers&Qt::AltModifier)mods|=4u;
    return set(id,sourceKey(key),mods);
}
bool UiShortcuts::clear(const QString &id){return set(id,0,0);}
bool UiShortcuts::reset(const QString &id){const int i=index(id);return i>=0 && set(id,base_[i].key,base_[i].mods);}
bool UiShortcuts::apply(){
    const auto fail=[&](const QString &message){editError_=message;Q_EMIT changed();return false;};
    if(overrides_.isEmpty())return fail("Shortcut saving is unavailable in this window.");
    if(!QDir().mkpath(QFileInfo(overrides_).absolutePath()))return fail("Cannot create shortcut settings folder.");
    QLockFile lock(overrides_+".lock");if(!lock.tryLock(0))return fail("Shortcut settings are being saved in another window. Try again.");
    if(bytes(path_)!=managedBytes_ || bytes(overrides_)!=overrideBytes_)return fail("Settings changed outside this editor. Close and reopen the app before editing again.");
    QString error;if(!validate(draft_,error))return fail(error);
    char message[256]={0};const auto encoded=QFile::encodeName(overrides_);
    if(!workbench_shortcuts_save(encoded.constData(),draft_.constData(),base_.constData(),draft_.size(),message,sizeof(message)))return fail(QString::fromUtf8(message));
    current_=draft_;overrideBytes_=bytes(overrides_);editError_.clear();rebuild();Q_EMIT changed();return true;
}
