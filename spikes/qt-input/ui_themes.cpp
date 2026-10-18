#include "ui_themes.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QSet>

namespace {
bool label(const QString &value,int maximum,bool optional=false) {
    if((!optional && value.trimmed().isEmpty()) || value.toUtf8().size()>maximum)return false;
    for(const auto ch:value)if(ch.unicode()<32 || ch.unicode()==127)return false;
    return true;
}
QString inferredName(const QString &source) {
    const QUrl url(source),download=UiThemes::downloadUrl(source);
    const auto path=!download.isEmpty()?download.path():url.scheme().isEmpty()?source:url.path();
    QString name=QFileInfo(path).completeBaseName();name.replace('-', ' ');name.replace('_',' ');
    if(name.isEmpty())name="Imported theme";
    auto words=name.split(' ',Qt::SkipEmptyParts);for(auto &word:words)word[0]=word[0].toUpper();
    return words.join(' ');
}
}
UiThemes::UiThemes(const QString &store,QObject *parent,QNetworkAccessManager *network,int timeoutMs,const QString &themesPath)
    :QObject(parent),themesPath_(themesPath),directory_(store),network_(network?network:new QNetworkAccessManager(this)) {
    deadline_.setSingleShot(true);deadline_.setInterval(timeoutMs);
    connect(&deadline_,&QTimer::timeout,this,[this]{fail("Theme download timed out. Existing themes were kept.");});
    for(int i=0;i<workbench_theme_builtin_count();++i)append(i);
    reloadDirectory();
    QVector<StoredTheme> loaded;
    if(!ThemeStore::load(directory_,loaded,storeError_)) {error_=storeError_;return;}
    QVector<WorkbenchTheme> colors;QVector<int> ignored;
    for(const auto &record:loaded) {
        WorkbenchTheme color;int count=0;
        if(!parse(record.text,color,count,storeError_)) {error_="Saved import is invalid: "+storeError_;storeError_=error_;return;}
        colors.append(color);ignored.append(count);
    }
    // No partial catalog on invalid saved data. Built-ins always remain available.
    records_=loaded;
    for(qsizetype i=0;i<loaded.size();++i) {
        const int index=workbench_theme_register(loaded[i].name.toUtf8().constData(),&colors[i]);
        if(index<0) {storeError_="Theme registration capacity reached.";error_=storeError_;return;}
        importIndices_.append(index);append(index,&loaded[i],ignored[i]);
    }
}
void UiThemes::reloadDirectory() {
    if(themesPath_.isEmpty())return;
    const QDir folder(themesPath_);
    if(!folder.isAbsolute()){directoryStatus_="Themes folder must be an absolute path.";Q_EMIT changed();return;}
    if(!folder.exists() && !QDir().mkpath(themesPath_)){directoryStatus_="Could not create the themes folder.";Q_EMIT changed();return;}
    const auto old=catalog_;QVariantList retained;
    for(const auto &value:catalog_)if(!value.toMap().value("folderTheme").toBool())retained.append(value);
    catalog_=retained;int loaded=0,skipped=0,examined=0;QString firstProblem;QSet<QString> seen;
    const auto files=folder.entryInfoList(QDir::Files|QDir::NoDotAndDotDot,QDir::Name|QDir::IgnoreCase);
    for(const auto &info:files) {
        if(++examined>256){firstProblem="Only the first 256 files are scanned.";break;}
        // Content determines validity: .default, .theme, .conf and extensionless files all work.
        QFile file(info.absoluteFilePath());QString problem;WorkbenchTheme colors;int ignored=0;
        if(!info.isFile() || info.size()>ThemeStore::maxBytes || !file.open(QIODevice::ReadOnly))problem="Cannot read file (maximum 64 KiB).";
        QByteArray bytes;
        if(problem.isEmpty()){bytes=file.read(ThemeStore::maxBytes+1);if(file.error()!=QFileDevice::NoError)problem="Could not read file.";}
        const bool valid=problem.isEmpty() && parse(bytes,colors,ignored,problem);
        if(!valid){++skipped;if(firstProblem.isEmpty())firstProblem=info.fileName()+": "+problem;continue;}
        const auto digest=ThemeStore::digest(bytes),name=inferredName(info.fileName());
        if(seen.contains(digest))continue;
        seen.insert(digest);
        if(!label(name,120)){++skipped;continue;}
        int index=directoryIndices_.value(digest,-1);
        if(index<0){index=workbench_theme_register(name.toUtf8().constData(),&colors);if(index>=0)directoryIndices_.insert(digest,index);}
        if(index<0){++skipped;firstProblem="Theme registry is full; reopen this window to load more changes.";continue;}
        const StoredTheme record{digest,name,QUrl::fromLocalFile(info.absoluteFilePath()).toString(),{},bytes};
        append(index,&record,ignored);
        auto entry=catalog_.takeLast().toMap();entry["folderTheme"]=true;catalog_.append(entry);lookup_[index]=entry;++loaded;
    }
    directoryStatus_=QString("%1 folder theme(s)").arg(loaded);
    if(skipped)directoryStatus_+=QString(" · %1 file(s) skipped").arg(skipped);
    if(!firstProblem.isEmpty())directoryStatus_+=" · "+firstProblem;
    // Removed/edited files leave old color slots alive for already-themed terminals.
    if(catalog_!=old)Q_EMIT catalogChanged();
    Q_EMIT changed();
}
void UiThemes::openThemesDirectory() {
    if(themesPath_.isEmpty())return;
    if(!QDir().mkpath(themesPath_) || !QDesktopServices::openUrl(QUrl::fromLocalFile(themesPath_))){directoryStatus_="Could not open themes folder: "+themesPath_;Q_EMIT changed();}
}
QVariantMap UiThemes::theme(int index) const {return lookup_.value(index,lookup_.value(0));}
void UiThemes::append(int index,const StoredTheme *record,int ignored) {
    WorkbenchTheme value;if(!workbench_theme_get(index,&value))return;
    const auto color=[](WorkbenchThemeColor c){return QColor(c.r,c.g,c.b);};
    const auto mix=[](QColor a,QColor b,qreal t){return QColor::fromRgbF(a.redF()*(1-t)+b.redF()*t,a.greenF()*(1-t)+b.greenF()*t,a.blueF()*(1-t)+b.blueF()*t);};
    const auto bg=color(value.background),fg=color(value.foreground);QVariantList swatches;
    for(int i=0;i<8;++i)swatches.append(color(value.palette[i]).name());
    QVariantMap entry{{"index",index},{"name",record?record->name:QString::fromUtf8(workbench_theme_name(index))},{"light",value.light},
        {"background",bg.name()},{"foreground",fg.name()},{"accent",color(value.cursor).name()},
        {"surface",mix(bg,fg,value.light?.035:.045).name()},{"hover",mix(bg,fg,value.light?.11:.14).name()},{"border",mix(bg,fg,value.light?.18:.19).name()},
        {"raised",mix(bg,fg,value.light?.06:.085).name()},
        {"selected",mix(bg,color(value.cursor),value.light?.13:.2).name()},
        {"waiting",value.light?"#835612":"#e6c689"},{"ready",value.light?"#2b704d":"#a5d2b1"},
        {"muted",mix(bg,fg,.65).name()},{"swatches",swatches},{"imported",record!=nullptr},{"ignored",ignored},
        {"source",record?record->source:QString()},{"attribution",record?record->attribution:QString()}};
    catalog_.append(entry);lookup_.insert(index,entry);
}
bool UiThemes::parse(const QByteArray &text,WorkbenchTheme &theme,int &ignored,QString &error) {
    if(text.isEmpty() || text.size()>ThemeStore::maxBytes || text.contains('\0')) {error="Choose a Ghostty color file of at most 64 KiB.";return false;}
    QStringDecoder decoder(QStringDecoder::Utf8);const QString decoded=decoder.decode(text);Q_UNUSED(decoded);
    if(decoder.hasError()) {error="Theme text must be valid UTF-8.";return false;}
    const auto colors=text.startsWith("\xef\xbb\xbf")?text.mid(3):text;
    char message[256]{};
    if(!workbench_theme_parse(colors.constData(),&theme,&ignored,message,sizeof(message))) {error=QString::fromUtf8(message);return false;}
    return true;
}
QUrl UiThemes::downloadUrl(const QString &source) {
    const QUrl url(source,QUrl::StrictMode);
    if(!url.isValid() || url.scheme()!="https" || url.host().toLower()!="terminalcolors.com" || url.port(-1)!=-1
        || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment())return {};
    static const QRegularExpression page("^/themes/([a-z0-9-]+)/([a-z0-9-]+)/?$");
    static const QRegularExpression file("^/downloads/ghostty/[a-z0-9-]+$");
    const auto match=page.match(url.path());
    if(match.hasMatch())return QUrl("https://terminalcolors.com/downloads/ghostty/"+match.captured(1)+'-'+match.captured(2));
    if(file.match(url.path()).hasMatch())return url;
    return {};
}
void UiThemes::clearError() {if(!busy_) {error_=storeError_;status_.clear();Q_EMIT changed();}}
void UiThemes::cancelImport() {
    deadline_.stop();auto old=reply_;reply_.clear();
    if(old) {old->abort();old->deleteLater();}
    download_.clear();busy_=false;Q_EMIT changed();
}
void UiThemes::fail(const QString &message) {cancelImport();error_=message;Q_EMIT changed();}
void UiThemes::importTheme(const QString &source,const QString &name,const QString &attribution) {
    if(busy_)return;
    error_.clear();status_.clear();
    if(!storeError_.isEmpty()) {fail(storeError_);return;}
    const auto display=name.trimmed().isEmpty()?inferredName(source):name.trimmed();
    if(!label(display,120) || !label(source,4096) || !label(attribution,1024,true)) {fail("Enter a theme name/source without control characters (name: 120 UTF-8 bytes).");return;}
    const QUrl input(source,QUrl::StrictMode),download=downloadUrl(source);
    if(input.scheme()=="https" || input.scheme()=="http") {
        if(download.isEmpty()) {fail("Use a TerminalColors theme/download link, or a local Ghostty file.");return;}
        busy_=true;pendingSource_=source;pendingName_=display;pendingAttribution_=attribution;download_.clear();
        QNetworkRequest request(download);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        request.setRawHeader("User-Agent","Terminal-Workbench-UI-Study/1");
        reply_=network_->get(request);reply_->setReadBufferSize(ThemeStore::maxBytes+1);const auto current=reply_;
        connect(current,&QNetworkReply::readyRead,this,&UiThemes::collect);
        connect(current,&QNetworkReply::finished,this,[this,current]{
            if(reply_!=current)return;
            collect();if(reply_!=current)return;
            const int code=current->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if(current->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {fail("Theme download redirected. Use the direct Ghostty download link.");return;}
            if(current->error()!=QNetworkReply::NoError || code!=200) {fail("Could not download theme: "+current->errorString());return;}
            const auto bytes=download_;download_.clear();deadline_.stop();reply_.clear();current->deleteLater();
            finish(bytes,pendingSource_,pendingName_,pendingAttribution_);
        });
        deadline_.start();Q_EMIT changed();return;
    }
    QString path;
    if(input.scheme().isEmpty())path=source;
    else if(input.isLocalFile())path=input.toLocalFile();
    else {fail("Choose a local Ghostty file or a TerminalColors link.");return;}
    if(path=="~")path=QDir::homePath();else if(path.startsWith("~/"))path=QDir::home().filePath(path.mid(2));
    const QFileInfo info(path);QFile file(info.absoluteFilePath());
    if(!info.isFile() || info.size()>ThemeStore::maxBytes || !file.open(QIODevice::ReadOnly)) {fail("Could not read theme file (maximum 64 KiB).");return;}
    const auto bytes=file.read(ThemeStore::maxBytes+1);
    if(file.error()!=QFileDevice::NoError) {fail("Could not read theme file.");return;}
    finish(bytes,QUrl::fromLocalFile(info.absoluteFilePath()).toString(),display,attribution);
}
void UiThemes::collect() {
    if(!reply_)return;
    if(reply_->header(QNetworkRequest::ContentLengthHeader).toLongLong()>ThemeStore::maxBytes) {fail("Theme download exceeds 64 KiB.");return;}
    download_+=reply_->read(ThemeStore::maxBytes+1-download_.size());
    if(download_.size()>ThemeStore::maxBytes)fail("Theme download exceeds 64 KiB.");
}
void UiThemes::finish(const QByteArray &text,const QString &source,const QString &name,const QString &attribution) {
    WorkbenchTheme color;int ignored=0;QString problem;
    if(!parse(text,color,ignored,problem)) {fail(problem);return;}
    const auto id=ThemeStore::digest(text);
    for(qsizetype i=0;i<records_.size();++i)if(records_[i].id==id) {
        busy_=false;status_="Already imported · "+records_[i].name;Q_EMIT changed();Q_EMIT imported(importIndices_[i]);return;
    }
    if(records_.size()>=ThemeStore::maxThemes) {fail("The imported theme limit is 64.");return;}
    StoredTheme record{id,name,source,attribution,text};QVector<StoredTheme> next;
    const auto validate=[](const QByteArray &bytes,QString &error){WorkbenchTheme parsed;int ignored=0;return parse(bytes,parsed,ignored,error);};
    if(!ThemeStore::insert(directory_,record,next,problem,validate)) {fail(problem);return;}
    QHash<QString,int> known;for(qsizetype i=0;i<records_.size();++i)known.insert(records_[i].id,importIndices_[i]);
    QVector<int> indices;int index=-1;
    for(const auto &saved:next) {
        int registered=known.value(saved.id,-1);
        if(registered<0) {
            WorkbenchTheme parsed;int skipped=0;
            if(!parse(saved.text,parsed,skipped,problem)) {fail(problem);return;}
            registered=workbench_theme_register(saved.name.toUtf8().constData(),&parsed);
            if(registered<0) {fail("Theme was saved but the presentation registry is full. Reopen the study to load it.");return;}
            append(registered,&saved,skipped);
        }
        indices.append(registered);if(saved.id==id)index=registered;
    }
    records_=next;importIndices_=indices;
    busy_=false;error_.clear();status_=ignored?QString("Imported · %1 non-color setting(s) ignored").arg(ignored):"Imported";
    Q_EMIT catalogChanged();Q_EMIT changed();Q_EMIT imported(index);
}
