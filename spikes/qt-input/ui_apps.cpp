#include "ui_apps.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <tuple>
#include <QRegularExpression>

namespace {
// Curated: package names checked against Homebrew, Fedora 44 (dnf repoquery),
// Debian and Arch (2026-11-07); empty where that manager has no package. COPR
// projects (owner/project, package named like the app) were current then;
// they are third-party and off until turned on in Sources.
struct Curated {const char *id,*name,*command,*description,*section,*brew,*dnf,*apt,*pacman,*copr;};
const Curated curated[]{
    {"lazygit","Lazygit","lazygit","Git in the terminal: stage, commit, rebase","Development","lazygit","","lazygit","lazygit","dejan/lazygit"},
    {"lazydocker","Lazydocker","lazydocker","Containers, images and logs","Docker/LXC/K8s","lazydocker","","","lazydocker",""},
    {"btop","btop","btop","Processes, CPU, memory, disks and network","Dashboards","btop","btop","btop","btop",""},
    {"htop","htop","htop","Interactive process viewer","Dashboards","htop","htop","htop","htop",""},
    {"yazi","Yazi","yazi","File manager with previews","File Managers","yazi","","","yazi","lihaohong/yazi"},
    {"neovim","Neovim","nvim","Editor","Editors","neovim","neovim","neovim","neovim",""},
    {"k9s","k9s","k9s","Kubernetes clusters","Docker/LXC/K8s","k9s","k9s","","k9s",""},
    {"tig","tig","tig","Git history browser","Development","tig","tig","tig","tig",""},
    {"ncdu","ncdu","ncdu","Disk usage by folder","Miscellaneous","ncdu","ncdu","ncdu","ncdu",""},
    {"gitui","GitUI","gitui","Git in the terminal, keyboard first","Development","gitui","gitui","","gitui",""},
};
// Homebrew's official installer, run in a visible pane (docs/apps.md).
const QString officialInstaller=QStringLiteral("/bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"");
bool loginShell(const QString &shell) {
    static const QStringList names{"zsh","bash","sh","fish","ksh","dash","csh","tcsh","nu"};
    return names.contains(QFileInfo(shell).fileName());
}
QString quoted(const QString &text) {return "'"+QString(text).replace("'","'\\''")+"'";}
}

QList<UiApps::Catalog> UiApps::catalog(bool apple,const QString &file) {
    QList<Catalog> result;QSet<QString> ids;
    for(const auto &c:curated){result.append({c.id,c.name,c.command,c.description,c.section,c.brew,c.dnf,c.apt,c.pacman,c.copr,true});ids.insert(c.id);}
    QFile source(file);
    if(!source.open(QIODevice::ReadOnly))return result;
    for(const auto &value:QJsonDocument::fromJson(source.readAll()).object()["apps"].toArray()) {
        const auto app=value.toObject();const auto id=app["id"].toString();
        if(id.isEmpty() || ids.contains(id) || !app[apple ? "mac" : "linux"].toBool(true) || program(app["command"].toString()).isEmpty())continue;
        ids.insert(id);
        result.append({id,app["name"].toString(id),app["command"].toString(),app["description"].toString(),app["section"].toString(),app["brew"].toString(),{},{},{},{},false});
    }
    return result;
}

QStringList UiApps::brewLocations() {
    return {"/opt/homebrew/bin/brew","/usr/local/bin/brew","/home/linuxbrew/.linuxbrew/bin/brew",QDir::home().filePath(".linuxbrew/bin/brew")};
}

UiApps::UiApps(bool apple,QString shell,QObject *parent,QString imageMarker,QString catalogFile,QString sourcesFile,QStringList brewAt)
    :QObject(parent),apple_(apple),shell_(std::move(shell)),imageMarker_(std::move(imageMarker)),sourcesFile_(std::move(sourcesFile)),brewAt_(std::move(brewAt)),known_(catalog(apple,catalogFile)) {
    QFile saved(sourcesFile_);
    if(saved.open(QIODevice::ReadOnly)) {
        const auto object=QJsonDocument::fromJson(saved.readAll()).object();
        for(auto it=object.begin();it!=object.end();++it)if(it.value().isBool())choices_.insert(it.key(),it.value().toBool());
    }
    deadline_.setSingleShot(true);deadline_.setInterval(15000);
    connect(&deadline_,&QTimer::timeout,this,[this]{if(process_)process_->kill();});
    rebuild();
}

QString UiApps::program(const QString &command) {
    const auto word=command.trimmed().section(QRegularExpression("\\s+"),0,0);
    static const QRegularExpression plain("^[A-Za-z0-9._+/~-]+$");
    if(!plain.match(word).hasMatch())return {};
    return word.startsWith("~/") ? QDir::home().filePath(word.mid(2)) : word;
}

QString UiApps::probeScript(const QStringList &programs,const QString &imageMarker,const QStringList &brewAt) {
    QString script;
    for(int i=0;i<programs.size();++i)
        if(!programs[i].isEmpty())script+=QStringLiteral("if command -v -- %1 >/dev/null 2>&1; then echo 'A %2 1'; else echo 'A %2 0'; fi\n").arg(quoted(programs[i])).arg(i);
    for(const auto *manager:{"brew","dnf","apt-get","pacman"})
        script+=QStringLiteral("command -v %1 >/dev/null 2>&1 && echo 'M %1'\n").arg(QLatin1String(manager));
    for(const auto &location:brewAt)script+=QStringLiteral("test -x %1 && echo %2\n").arg(quoted(location),quoted("H "+location));
    return script+QStringLiteral("test -e %1 && echo 'I'\nexit 0\n").arg(quoted(imageMarker));
}

void UiApps::setDefinitions(const QVariantList &definitions) {
    if(definitions==definitions_)return;
    definitions_=definitions;rebuild();refresh();
}

QVariantMap UiApps::entry(const QString &id) const {
    for(const auto &value:entries_)if(value.toMap()["id"]==id)return value.toMap();
    return {};
}

void UiApps::refresh() {
    if(checking_){again_=true;return;}
    checking_=true;Q_EMIT checkingChanged();
    // The login shell's PATH, as apps get it; env is an external command, so
    // this works for any shell. Its other output is ignored.
    process_=new QProcess(this);
    QStringList arguments;if(loginShell(shell_))arguments<<"-l";arguments<<"-c"<<"env";
    process_->setProgram(shell_);process_->setArguments(arguments);
    process_->setStandardInputFile(QProcess::nullDevice());process_->setStandardErrorFile(QProcess::nullDevice());
    connect(process_,&QProcess::finished,this,[this]{
        const auto output=process_->readAllStandardOutput();process_->deleteLater();process_=nullptr;deadline_.stop();
        QByteArray path=qgetenv("PATH");
        for(const auto &line:output.split('\n'))if(line.startsWith("PATH="))path=line.mid(5);
        probe(path);
    });
    connect(process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){process_->deleteLater();process_=nullptr;deadline_.stop();probe(qgetenv("PATH"));}});
    deadline_.start();process_->start();
}

void UiApps::probe(const QByteArray &path) {
    programs_.clear();
    for(const auto &value:entries_)programs_<<program(value.toMap()["command"].toString());
    process_=new QProcess(this);
    auto environment=QProcessEnvironment::systemEnvironment();environment.insert("PATH",QString::fromLocal8Bit(path));
    process_->setProcessEnvironment(environment);
    process_->setProgram("/bin/sh");process_->setArguments({"-c",probeScript(programs_,imageMarker_,brewAt_)});
    process_->setStandardInputFile(QProcess::nullDevice());process_->setStandardErrorFile(QProcess::nullDevice());
    connect(process_,&QProcess::finished,this,[this]{const auto output=process_->readAllStandardOutput();process_->deleteLater();process_=nullptr;deadline_.stop();finish(output);});
    connect(process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){process_->deleteLater();process_=nullptr;deadline_.stop();finish({});}});
    deadline_.start();process_->start();
}

void UiApps::finish(const QByteArray &output) {
    installed_.clear();managers_.clear();image_=false;brewOffPath_.clear();
    for(const auto &line:output.split('\n')) {
        const auto parts=QString::fromUtf8(line).split(' ');
        if(parts.size()==3 && parts[0]=="A") {const int i=parts[1].toInt();if(i>=0 && i<programs_.size())installed_.insert(programs_[i],parts[2]=="1");}
        else if(parts.size()==2 && parts[0]=="M")managers_.insert(parts[1]);
        else if(parts.size()==1 && parts[0]=="I")image_=true;
        else if(line.startsWith("H ") && brewOffPath_.isEmpty())brewOffPath_=QString::fromUtf8(line.mid(2));
    }
    checked_=!output.isEmpty();checking_=false;rebuild();Q_EMIT checkingChanged();Q_EMIT checked();
    if(again_){again_=false;refresh();}
}

bool UiApps::enabled(const QString &source) const {
    // Extra repositories are third-party: off until turned on.
    return choices_.value(source,source!="copr");
}

void UiApps::setSourceEnabled(const QString &id,bool on) {
    if(enabled(id)==on)return;
    choices_.insert(id,on);
    if(!sourcesFile_.isEmpty()) {
        QJsonObject object;for(auto it=choices_.begin();it!=choices_.end();++it)object.insert(it.key(),it.value());
        QDir().mkpath(QFileInfo(sourcesFile_).absolutePath());
        QSaveFile file(sourcesFile_);
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(object).toJson());file.commit();}
    }
    rebuild();
}

void UiApps::rebuild() {
    QVariantList result;QHash<QString,QVariantMap> custom;QStringList order;
    for(const auto &value:definitions_){const auto map=value.toMap();custom.insert(map["id"].toString(),map);order<<map["id"].toString();}
    const auto add=[&](QVariantMap entry,const Catalog *known){
        const auto run=program(entry["command"].toString());
        // Unknown until a check has looked at this program.
        entry.insert("installed",!checked_ || !installed_.contains(run) ? -1 : installed_[run] ? 1 : 0);
        entry.insert("builtin",known!=nullptr);
        entry.insert("featured",known && known->featured);
        if(!entry.contains("section"))entry.insert("section",QString());
        // Every way to install it here, in order of preference; turned-off
        // sources are left out and named in the hint instead.
        // Your own recipe ([app.ID] install) comes first; setup runs after
        // whichever recipe succeeds.
        const auto own=entry.value("install").toString(),setup=entry.value("setup").toString();
        entry.insert("setup",setup);
        QVariantList options;QStringList off;
        const auto offer=[&](const QString &source,const QString &via,const QString &install){
            if(enabled(source))options<<QVariantMap{{"source",source},{"via",via},{"install",setup.isEmpty() ? install : install+" && "+setup}};else off<<via;
        };
        if(!own.isEmpty() && checked_)options<<QVariantMap{{"source","own"},{"via","your recipe"},{"install",setup.isEmpty() ? own : own+" && "+setup}};
        if(known && checked_) {
            if(managers_.contains("brew") && !known->brew.isEmpty())offer("brew","Homebrew","brew install "+known->brew);
            if(!apple_ && !image_) {
                if(managers_.contains("dnf") && !known->dnf.isEmpty())offer("dnf","dnf","sudo dnf install "+known->dnf);
                if(managers_.contains("apt-get") && !known->apt.isEmpty())offer("apt","apt","sudo apt install "+known->apt);
                if(managers_.contains("pacman") && !known->pacman.isEmpty())offer("pacman","pacman","sudo pacman -S "+known->pacman);
                if(managers_.contains("dnf") && !known->copr.isEmpty())offer("copr","COPR "+known->copr,"sudo dnf copr enable "+known->copr+" && sudo dnf install "+known->id);
            }
        }
        const auto first=options.isEmpty() ? QVariantMap() : options.first().toMap();
        entry.insert("options",options);entry.insert("install",first["install"]);entry.insert("via",first["via"]);
        // Homebrew would offer it once set up (or loaded by the shell).
        entry.insert("needsHomebrew",known && checked_ && options.isEmpty() && !known->brew.isEmpty() && !managers_.contains("brew"));
        entry.insert("hint",!known ? (options.isEmpty() && checked_ ? QStringLiteral("Defined in config.conf without install; add one to install it from Band.") : QString())
            : !options.isEmpty() || !checked_ ? QString()
            : !known->brew.isEmpty() && !managers_.contains("brew") ? (!brewOffPath_.isEmpty() ? QStringLiteral("Homebrew has it; Homebrew is installed but your shell does not load it yet. See Sources.") : QStringLiteral("Homebrew has it; set up Homebrew in Sources."))
                + (off.isEmpty() ? QString() : QStringLiteral(" Also in %1, turned off in Sources.").arg(off.join(", ")))
            : !off.isEmpty() ? QStringLiteral("Available from %1, which is turned off in Sources.").arg(off.join(", "))
            : image_ ? QStringLiteral("On an image-based system, use Homebrew or a toolbox.")
            : QStringLiteral("Not packaged for this system's package managers."));
        result<<entry;
    };
    // Your own definitions first, then the catalog; a definition with a
    // catalog ID overrides its name, command and folder.
    for(const auto &id:order)if(std::none_of(known_.begin(),known_.end(),[&](const Catalog &c){return id==c.id;}))add(custom[id],nullptr);
    for(const auto &known:known_) {
        QVariantMap entry{{"id",known.id},{"name",known.name},{"command",known.command},{"directory",QString()},{"description",known.description},{"section",known.section}};
        if(custom.contains(known.id))for(const auto &key:{"name","command","directory","install","setup"})entry.insert(key,custom[known.id][key]);
        add(entry,&known);
    }
    // Sources: Homebrew everywhere; on Linux the system package manager found
    // and, with dnf, COPR repositories.
    const bool brew=managers_.contains("brew");
    const auto shellName=QFileInfo(shell_).fileName();
    const auto shellenv=brewOffPath_.isEmpty() ? QString() : shellName=="fish" ? brewOffPath_+" shellenv | source" : "eval \"$("+brewOffPath_+" shellenv)\"";
    const QVariantMap homebrew{{"found",brew},{"path",brewOffPath_},{"shellenv",shellenv},{"install",installer_.isEmpty() ? officialInstaller : installer_},{"checked",checked_}};
    QVariantList sources;
    sources<<QVariantMap{{"id","brew"},{"name","Homebrew"},{"found",brew},{"enabled",enabled("brew")},
        {"detail",brew ? QStringLiteral("Installs apps for your user, on macOS and Linux.") : !brewOffPath_.isEmpty() ? QStringLiteral("Installed at %1, but your shell does not load it yet.").arg(brewOffPath_) : QStringLiteral("Not installed. Most apps in the catalog install with Homebrew.")}};
    if(!apple_) {
        const QList<std::tuple<QString,QString,QString>> system{{"dnf","dnf","Fedora's package manager; asks for your password."},{"apt-get","apt","Debian and Ubuntu's package manager; asks for your password."},{"pacman","pacman","Arch's package manager; asks for your password."}};
        for(const auto &[command,id,detail]:system)if(managers_.contains(command))
            sources<<QVariantMap{{"id",id},{"name",id},{"found",!image_},{"enabled",enabled(id)},{"detail",image_ ? QStringLiteral("Image-based system: Band does not install system packages here.") : detail}};
        if(managers_.contains("dnf") && !image_)
            sources<<QVariantMap{{"id","copr"},{"name","COPR repositories"},{"found",true},{"enabled",enabled("copr")},{"detail",QStringLiteral("Third-party Fedora repositories, for apps Fedora lacks. Each is named before it is added.")}};
    }
    const QString note=!checked_ || brew || !apple_ ? QString() : QStringLiteral("Homebrew was not found. Set it up in Sources to install apps here.");
    // An unchanged list keeps the panel's rows (and the pointer's target) as they are.
    if(result!=entries_){entries_=result;Q_EMIT entriesChanged();}
    if(note!=note_ || sources!=sources_ || homebrew!=homebrew_){note_=note;sources_=sources;homebrew_=homebrew;Q_EMIT sourcesChanged();}
}
