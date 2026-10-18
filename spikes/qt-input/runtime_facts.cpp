#include "runtime_facts.h"
#include <QDir>
#include <QFile>
#include <QStringDecoder>
#include <QtEndian>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool ownedSocket(const QString &path) {
#ifdef Q_OS_UNIX
    struct stat info{};
    return ::lstat(QFile::encodeName(path).constData(),&info)==0 && S_ISSOCK(info.st_mode) && info.st_uid==::geteuid();
#else
    Q_UNUSED(path);return false;
#endif
}
RuntimeFeed::RuntimeFeed(QByteArray request,quint8 kind,QObject *parent,
    std::function<bool(const QByteArray &)> consume,std::function<void()> unavailable)
    :QObject(parent),request_(std::move(request)),kind_(kind),consume_(std::move(consume)),unavailable_(std::move(unavailable)) {
    socket_.setReadBufferSize(32768+5);deadline_.setSingleShot(true);deadline_.setInterval(2500);
    connect(&socket_,&QLocalSocket::connected,this,[this]{
        if(!failed_ && socket_.write(request_)!=request_.size())fail();
    });
    connect(&socket_,&QLocalSocket::readyRead,this,[this]{drain();});
    connect(&socket_,&QLocalSocket::disconnected,this,[this]{fail();});
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](auto){fail();});
    connect(&deadline_,&QTimer::timeout,this,[this]{fail();});
}
RuntimeFeed::~RuntimeFeed() {socket_.disconnect(this);socket_.abort();}
void RuntimeFeed::open(const QString &path) {
    if(!failed_)return;
    buffer_.clear();
    if(!ownedSocket(path)) {unavailable_();return;}
    failed_=false;deadline_.start();socket_.connectToServer(path,QIODevice::ReadWrite);
}
void RuntimeFeed::fail() {
    if(failed_)return;
    failed_=true;deadline_.stop();socket_.abort();buffer_.clear();unavailable_();
}
void RuntimeFeed::drain() {
    // Consume incrementally even when multiple complete frames arrive together.
    while(!failed_ && socket_.bytesAvailable()>0) {
        const qint64 needed=buffer_.size()<5 ? 5-buffer_.size() : 5+qFromBigEndian<quint32>(buffer_.constData()+1)-buffer_.size();
        buffer_+=socket_.read(needed);
        if(buffer_.size()<5)return;
        const auto size=qFromBigEndian<quint32>(buffer_.constData()+1);
        if(quint8(buffer_[0])!=kind_ || size>32768 || (kind_==6 && size!=2)) {fail();return;}
        if(buffer_.size()<5+qsizetype(size))continue;
        if(!consume_(buffer_.mid(5))) {fail();return;}
        deadline_.stop();buffer_.clear();
    }
}
RuntimeFacts::RuntimeFacts(const QString &path,QObject *parent):QObject(parent),path_(path),
    contextFeed_("CTX1\n",10,this,[this](const auto &p){return context(p);},[this]{clearContext();}),
    agentFeed_("WAT2\n",6,this,[this](const auto &p){return agent(p);},[this]{clearAgent();}) {
    // Defer until construction is complete; callbacks may emit changed().
    QTimer::singleShot(0,this,[this]{refresh();});
}
void RuntimeFacts::refresh() {contextFeed_.open(path_);agentFeed_.open(path_);}
void RuntimeFacts::clearContext() {
    if(!contextAvailable_ && !remote_ && !detached_ && cwd_.isEmpty() && repository_.isEmpty() && branch_.isEmpty() && worktree_.isEmpty())return;
    cwd_.clear();repository_.clear();branch_.clear();worktree_.clear();contextAvailable_=remote_=detached_=false;Q_EMIT changed();
}
void RuntimeFacts::clearAgent() {
    if(!agentAvailable_ && agent_==255 && state_==0)return;
    agentAvailable_=false;agent_=255;state_=0;Q_EMIT changed();
}
bool RuntimeFacts::context(const QByteArray &payload) {
    if(payload.size()<10 || payload[0]!=1 || (quint8(payload[1])&~7))return false;
    QString values[4];qsizetype offset=2;
    for(auto &value:values) {
        if(offset+2>payload.size())return false;
        const auto length=qFromBigEndian<quint16>(payload.constData()+offset);offset+=2;
        if(length>4096 || offset+length>payload.size())return false;
        QStringDecoder decoder(QStringDecoder::Utf8);value=decoder.decode(payload.mid(offset,length));
        if(decoder.hasError() || value.contains(QChar(0)))return false;
        // Filenames can contain newlines. Display controls as spaces without rewriting runtime facts.
        for(auto &ch:value)if(ch.category()==QChar::Other_Control)ch=QChar(' ');
        offset+=length;
    }
    if(offset!=payload.size())return false;
    const auto flags=quint8(payload[1]);const bool available=(flags&1) && !(flags&4),remote=flags&4,detached=flags&2;
    if(!available)for(auto &value:values)value.clear();
    if(cwd_==values[0] && repository_==values[1] && branch_==values[2] && worktree_==values[3] && contextAvailable_==available && remote_==remote && detached_==detached)return true;
    cwd_=values[0];repository_=values[1];branch_=values[2];worktree_=values[3];contextAvailable_=available;remote_=remote;detached_=detached;
    Q_EMIT changed();return true;
}
bool RuntimeFacts::agent(const QByteArray &payload) {
    if(payload.size()!=2)return false;
    const auto agent=quint8(payload[0]),state=quint8(payload[1]);
    if((agent>6 && agent!=255) || state>3)return false;
    if(agent_==agent && state_==state && agentAvailable_==(agent!=255))return true;
    agent_=agent;state_=state;agentAvailable_=agent!=255;Q_EMIT changed();return true;
}
QVariantMap RuntimeFacts::facts() const {
    static const QStringList agents={"","Codex","Claude","Gemini","OpenCode","Aider","Copilot"};
    static const QStringList states={"Status unknown","Working","Needs input","Ready"};
    QString title=remote_ ? "Remote session" : "Terminal";
    if(contextAvailable_) {
        title=cwd_;
        const auto home=QDir::homePath();
        if(title==home)title="~";else if(title.startsWith(home+'/'))title='~'+title.mid(home.size());
        if(!repository_.isEmpty()) {
            title=repository_;if(!branch_.isEmpty())title+=" · "+branch_;
            if(!worktree_.isEmpty())title+=" · "+worktree_;
            if(detached_)title+=" · detached";
        }
    }
    return {{"title",title},{"cwd",cwd_},{"repository",repository_},{"branch",branch_},{"worktree",worktree_},
        {"context_available",contextAvailable_},{"remote",remote_},{"detached",detached_},
        {"agent_available",agentAvailable_},{"agent",agent_<=6 ? agents[agent_] : QString()},
        {"state",agent_>0 && agent_<=6 ? states[state_] : QString()},
        {"needs_input",agent_>0 && agent_<=6 && state_==2}};
}
