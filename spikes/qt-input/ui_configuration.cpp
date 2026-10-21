#include "ui_configuration.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

UiConfiguration::UiConfiguration(UiFonts &fonts,UiShortcuts &shortcuts,bool watch,QObject *parent,UiAppearance *appearance,bool apple)
    :QObject(parent),appearance_(appearance),apple_(apple),creation_(CreationDefaults::read(fonts.managedPath(),apple)),view_(ViewDefaults::read(fonts.managedPath(),apple)),fonts_(fonts),shortcuts_(shortcuts),watching_(watch)
{
    if(appearance_)error_=appearance_->error();
    if(!creation_.error.isEmpty())error_+=(error_.isEmpty()?QString():QString(" "))+creation_.error;
    if(!view_.error.isEmpty() && !error_.contains(view_.error))error_+=(error_.isEmpty()?QString():QString(" "))+view_.error;
    attempted_=snapshot();debounce_.setSingleShot(true);debounce_.setInterval(150);
    connect(&watcher_,&QFileSystemWatcher::fileChanged,this,[this]{debounce_.start();});
    connect(&watcher_,&QFileSystemWatcher::directoryChanged,this,[this]{debounce_.start();});
    connect(&debounce_,&QTimer::timeout,this,[this]{refresh(false);});
    if(watching_)watchSources();
}
QStringList UiConfiguration::sources() const
{
    QStringList paths{managedPath(),fontPath(),shortcutPath()};if(appearance_)paths.append(appearance_->sourcePath());paths.removeAll(QString());paths.removeDuplicates();return paths;
}
QMap<QString,QByteArray> UiConfiguration::snapshot() const
{
    QMap<QString,QByteArray> result;
    for(const auto &path:sources()) {
        const QFileInfo info(path);
        if(!info.isFile()){result[path]=info.exists() || info.isSymLink()?QByteArray("not-file"):QByteArray("missing");continue;}
        QFile file(path);
        result[path]=file.open(QIODevice::ReadOnly)?QByteArray("file:")+file.read(1024*1024+1):QFileInfo::exists(path)?QByteArray("error:")+file.errorString().toUtf8():QByteArray("missing");
    }
    return result;
}
void UiConfiguration::watchSources()
{
    const auto files=watcher_.files(),directories=watcher_.directories();
    if(!files.isEmpty())watcher_.removePaths(files);
    if(!directories.isEmpty())watcher_.removePaths(directories);
    QStringList paths;
    for(const auto &source:sources()) {
        const QFileInfo info(source);if(info.isFile())paths.append(info.absoluteFilePath());
        QStringList targets{info.absoluteFilePath()};const auto resolved=info.canonicalFilePath();if(!resolved.isEmpty())targets.append(resolved);
        // A dangling theme symlink still needs its target directory watched so
        // creating the target can repair the last failed reload.
        if(info.isSymLink())targets.append(info.symLinkTarget());
        for(const auto &target:targets) {
            QDir parent(QFileInfo(target).absolutePath());while(!parent.exists() && parent.cdUp()){}
            if(parent.exists())paths.append(parent.absolutePath());
        }
    }
    paths.removeDuplicates();if(!paths.isEmpty())watcher_.addPaths(paths);
}
void UiConfiguration::setEditing(bool editing)
{
    if(editing_==editing)return;
    editing_=editing;Q_EMIT changed();
    if(!editing_ && pending_)debounce_.start();
}
bool UiConfiguration::reload(){return refresh(true);}
bool UiConfiguration::refresh(bool manual)
{
    if(watching_)watchSources();
    if(editing_) {pending_=true;status_="Changes waiting until settings close";Q_EMIT changed();return false;}
    const auto before=snapshot();
    if(!manual && before==attempted_){pending_=false;return true;}
    auto fonts=fonts_.reloadCandidate();auto shortcuts=shortcuts_.reloadCandidate();
    const auto appearance=appearance_?appearance_->prepare():UiAppearance::Candidate{};
    const auto creation=CreationDefaults::read(managedPath(),apple_);
    const auto view=ViewDefaults::read(managedPath(),apple_);
    if(before!=snapshot()){pending_=true;debounce_.start();return false;}
    attempted_=before;pending_=false;
    QStringList errors;if(!fonts->error().isEmpty())errors.append(fonts->error());if(!shortcuts->error().isEmpty())errors.append(shortcuts->error());if(!appearance.error.isEmpty())errors.append(appearance.error);if(!creation.error.isEmpty())errors.append(creation.error);if(!view.error.isEmpty())errors.append(view.error);errors.removeDuplicates();
    if(!errors.isEmpty()) {error_="Settings kept. "+errors.join(" ");status_="Reload failed";Q_EMIT changed();Q_EMIT reloadFailed();return false;}
    if(appearance_ && !appearance_->adopt(appearance)){error_="Settings kept. "+appearance_->error();status_="Reload failed";Q_EMIT changed();Q_EMIT reloadFailed();return false;}
    creation_=creation;view_=view;
    fonts_.adoptReload(*fonts);shortcuts_.adoptReload(*shortcuts);
    error_.clear();status_="Preferences reloaded";
    // All domains are installed before observers are notified.
    if(appearance_)Q_EMIT appearance_->changed();
    Q_EMIT fonts_.changed();Q_EMIT shortcuts_.changed();Q_EMIT changed();return true;
}
void UiConfiguration::openDirectory()
{
    const auto directory=QFileInfo(managedPath()).absolutePath();
    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(directory))) {error_="Could not open the configuration folder";Q_EMIT changed();Q_EMIT reloadFailed();}
}
