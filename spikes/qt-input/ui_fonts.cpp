#include "ui_fonts.h"
#include "font_bridge.h"
#include <QFile>
#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
namespace {
QByteArray bytes(const QString &path,qint64 limit=4097){QFile file(path);return file.open(QIODevice::ReadOnly)?file.read(limit):QByteArray();}
bool valid(const QString &family,int size){return (family=="Fira Code" || family=="JetBrains Mono") && size>=10 && size<=32;}
}
UiFonts::UiFonts(const QString &path,bool apple,QObject *parent,const QString &overrides):QObject(parent),path_(path),overrides_(overrides),apple_(apple)
{
    int family=0,size=16;char error[256]={0};const auto encoded=QFile::encodeName(path);
    if(!workbench_font_defaults(path.isEmpty()?nullptr:encoded.constData(),apple,&family,&size,error,sizeof(error)))error_="Using built-in fonts. "+QString::fromUtf8(error);
    defaults_={{"family",family==1?"Fira Code":"JetBrains Mono"},{"size",size}};
    current_=defaults_;managedBytes_=bytes(path_,1048577);overrideBytes_=bytes(overrides_);
    if(!overrides_.isEmpty() && (QFileInfo::exists(overrides_) || QFileInfo(overrides_).isSymLink())){
        QFile file(overrides_);QJsonParseError parse;
        const auto document=QJsonDocument::fromJson(overrideBytes_,&parse);const auto object=document.object();
        auto next=defaults_;bool ok=file.open(QIODevice::ReadOnly) && overrideBytes_.size()<=4096 && parse.error==QJsonParseError::NoError && document.isObject() && object["version"].toDouble(-1)==1;
        for(auto it=object.begin();it!=object.end();++it){
            if(it.key()=="version")continue;
            if(it.key()=="family" && it.value().isString())next["family"]=it.value().toString();
            else if(it.key()=="size" && it.value().isDouble() && it.value().toDouble()==it.value().toInt())next["size"]=it.value().toInt();
            else ok=false;
        }
        if(ok && valid(next["family"].toString(),next["size"].toInt()))current_=next;
        else {saveAllowed_=false;error_+=(error_.isEmpty()?QString():QStringLiteral(" "))+QStringLiteral("Using managed font defaults. Cannot read saved font settings; file left untouched.");}
    }
    original_=current_;persisted_=current_;
}
bool UiFonts::preview(const QString &family,int size)
{
    if(!valid(family,size))return false;
    const QVariantMap next{{"family",family},{"size",size}};
    if(current_!=next){current_=next;Q_EMIT changed();}return true;
}
void UiFonts::cancel(){preview(original_["family"].toString(),original_["size"].toInt());}
void UiFonts::useDefaults(){preview(defaults_["family"].toString(),defaults_["size"].toInt());}
void UiFonts::step(int direction){preview(current_["family"].toString(),std::clamp(current_["size"].toInt()+(direction>0?1:direction<0?-1:0),10,32));}
void UiFonts::resetSize(){preview(current_["family"].toString(),defaults_["size"].toInt());}

bool UiFonts::apply()
{
    const auto fail=[&](const QString &message){editError_=message;Q_EMIT changed();return false;};
    // Standalone probes may intentionally omit durable preference storage.
    if(overrides_.isEmpty()){original_=current_;persisted_=current_;return true;}
    if(!saveAllowed_)return fail("Saved font settings are invalid. Correct the file and reopen the app before saving.");
    if(!QDir().mkpath(QFileInfo(overrides_).absolutePath()))return fail("Cannot create font settings folder.");
    QLockFile lock(overrides_+".lock");if(!lock.tryLock(0))return fail("Font settings are being saved in another window. Try again.");
    if(bytes(path_,1048577)!=managedBytes_ || bytes(overrides_)!=overrideBytes_)return fail("Font settings changed outside this window. Reopen the app before saving.");
    QJsonObject object{{"version",1}};
    for(const auto &field:{QStringLiteral("family"),QStringLiteral("size")})if(current_[field]!=defaults_[field])object[field]=QJsonValue::fromVariant(current_[field]);
    const auto encoded=QJsonDocument(object).toJson();QSaveFile file(overrides_);
    if(!file.open(QIODevice::WriteOnly))return fail("Cannot save font settings: "+file.errorString());
    if(!file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner))return fail("Cannot set font settings permissions.");
    if(file.write(encoded)!=encoded.size() || !file.commit())return fail("Cannot save font settings: "+file.errorString());
    overrideBytes_=encoded;original_=current_;persisted_=current_;editError_.clear();Q_EMIT changed();return true;
}

std::unique_ptr<UiFonts> UiFonts::reloadCandidate() const
{
    return std::make_unique<UiFonts>(path_,apple_,nullptr,overrides_);
}
void UiFonts::adoptReload(const UiFonts &candidate)
{
    // Temporary font-size shortcuts stay window-local when persisted values
    // did not change (for example, a peer only saved new shortcuts).
    if(persisted_!=candidate.persisted_)current_=candidate.current_;
    persisted_=candidate.persisted_;defaults_=candidate.defaults_;original_=current_;
    managedBytes_=candidate.managedBytes_;overrideBytes_=candidate.overrideBytes_;
    saveAllowed_=candidate.saveAllowed_;error_=candidate.error_;editError_.clear();
}
