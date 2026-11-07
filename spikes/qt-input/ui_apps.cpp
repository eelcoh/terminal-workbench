#include "ui_apps.h"
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace {
// Package names checked against Homebrew, Fedora 43, Debian and Arch (2026-11-06);
// empty where that manager has no package.
const QList<UiApps::Catalog> builtIn{
    {"lazygit","Lazygit","lazygit","Git in the terminal: stage, commit, rebase","lazygit","lazygit","lazygit","lazygit"},
    {"lazydocker","Lazydocker","lazydocker","Containers, images and logs","lazydocker","","","lazydocker"},
    {"btop","btop","btop","Processes, CPU, memory, disks and network","btop","btop","btop","btop"},
    {"htop","htop","htop","Interactive process viewer","htop","htop","htop","htop"},
    {"yazi","Yazi","yazi","File manager with previews","yazi","","","yazi"},
    {"neovim","Neovim","nvim","Editor","neovim","neovim","neovim","neovim"},
    {"k9s","k9s","k9s","Kubernetes clusters","k9s","k9s","","k9s"},
    {"tig","tig","tig","Git history browser","tig","tig","tig","tig"},
    {"ncdu","ncdu","ncdu","Disk usage by folder","ncdu","ncdu","ncdu","ncdu"},
    {"gitui","GitUI","gitui","Git in the terminal, keyboard first","gitui","gitui","","gitui"},
};
bool loginShell(const QString &shell) {
    static const QStringList names{"zsh","bash","sh","fish","ksh","dash","csh","tcsh","nu"};
    return names.contains(QFileInfo(shell).fileName());
}
QString quoted(const QString &text) {return "'"+QString(text).replace("'","'\\''")+"'";}
}

const QList<UiApps::Catalog> &UiApps::catalog() {return builtIn;}

UiApps::UiApps(bool apple,QString shell,QObject *parent,QString imageMarker)
    :QObject(parent),apple_(apple),shell_(std::move(shell)),imageMarker_(std::move(imageMarker)) {
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

QString UiApps::probeScript(const QStringList &programs,const QString &imageMarker) {
    QString script;
    for(int i=0;i<programs.size();++i)
        if(!programs[i].isEmpty())script+=QStringLiteral("if command -v -- %1 >/dev/null 2>&1; then echo 'A %2 1'; else echo 'A %2 0'; fi\n").arg(quoted(programs[i])).arg(i);
    for(const auto *manager:{"brew","dnf","apt-get","pacman"})
        script+=QStringLiteral("command -v %1 >/dev/null 2>&1 && echo 'M %1'\n").arg(QLatin1String(manager));
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
    checking_=true;Q_EMIT changed();
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
    process_->setProgram("/bin/sh");process_->setArguments({"-c",probeScript(programs_,imageMarker_)});
    process_->setStandardInputFile(QProcess::nullDevice());process_->setStandardErrorFile(QProcess::nullDevice());
    connect(process_,&QProcess::finished,this,[this]{const auto output=process_->readAllStandardOutput();process_->deleteLater();process_=nullptr;deadline_.stop();finish(output);});
    connect(process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){process_->deleteLater();process_=nullptr;deadline_.stop();finish({});}});
    deadline_.start();process_->start();
}

void UiApps::finish(const QByteArray &output) {
    installed_.clear();managers_.clear();image_=false;
    for(const auto &line:output.split('\n')) {
        const auto parts=QString::fromUtf8(line).split(' ');
        if(parts.size()==3 && parts[0]=="A") {const int i=parts[1].toInt();if(i>=0 && i<programs_.size())installed_.insert(programs_[i],parts[2]=="1");}
        else if(parts.size()==2 && parts[0]=="M")managers_.insert(parts[1]);
        else if(parts.size()==1 && parts[0]=="I")image_=true;
    }
    checked_=!output.isEmpty();checking_=false;Q_EMIT changed();rebuild();Q_EMIT checked();
    if(again_){again_=false;refresh();}
}

void UiApps::rebuild() {
    QVariantList result;QHash<QString,QVariantMap> custom;QStringList order;
    for(const auto &value:definitions_){const auto map=value.toMap();custom.insert(map["id"].toString(),map);order<<map["id"].toString();}
    const auto add=[&](QVariantMap entry,const Catalog *known){
        const auto run=program(entry["command"].toString());
        // Unknown until a check has looked at this program.
        entry.insert("installed",!checked_ || !installed_.contains(run) ? -1 : installed_[run] ? 1 : 0);
        entry.insert("builtin",known!=nullptr);
        QString install,via;
        if(known && checked_) {
            if(managers_.contains("brew") && *known->brew){install="brew install "+QLatin1String(known->brew);via="Homebrew";}
            else if(!apple_ && !image_) {
                if(managers_.contains("dnf") && *known->dnf){install="sudo dnf install "+QLatin1String(known->dnf);via="dnf";}
                else if(managers_.contains("apt-get") && *known->apt){install="sudo apt install "+QLatin1String(known->apt);via="apt";}
                else if(managers_.contains("pacman") && *known->pacman){install="sudo pacman -S "+QLatin1String(known->pacman);via="pacman";}
            }
        }
        entry.insert("install",install);entry.insert("via",via);
        entry.insert("hint",!known ? QStringLiteral("Defined in config.conf; install it yourself.")
            : !install.isEmpty() || !checked_ ? QString()
            : apple_ ? QStringLiteral("Install Homebrew (brew.sh) to install it from Band.")
            : image_ ? QStringLiteral("On an image-based system, install Homebrew or use a toolbox.")
            : QStringLiteral("Not packaged for this system's package manager; Homebrew has it."));
        result<<entry;
    };
    // Your own definitions first, then the catalog; a definition with a
    // catalog ID overrides its name, command and folder.
    for(const auto &id:order)if(std::none_of(builtIn.begin(),builtIn.end(),[&](const Catalog &c){return id==QLatin1String(c.id);}))add(custom[id],nullptr);
    for(const auto &known:builtIn) {
        QVariantMap entry{{"id",known.id},{"name",known.name},{"command",known.command},{"directory",QString()},{"description",known.description}};
        if(custom.contains(known.id))for(const auto &key:{"name","command","directory"})entry.insert(key,custom[known.id][key]);
        add(entry,&known);
    }
    const QString note=!checked_ || managers_.contains("brew") || !apple_ ? QString() : QStringLiteral("Homebrew was not found; install it from brew.sh to install apps here.");
    // An unchanged list keeps the panel's rows (and the pointer's target) as they are.
    if(result==entries_ && note==note_)return;
    entries_=result;note_=note;
    Q_EMIT changed();
}
