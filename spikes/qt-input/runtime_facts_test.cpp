#include "runtime_facts.h"
#include <QCoreApplication>
#include <QLocalServer>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QFile>
#include <QtEndian>
#include <cstdio>
#include <cstdlib>
static void require(bool result,const char *why) {if(!result){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static bool wait(const std::function<bool()> &condition,int timeout=4000) {
    QElapsedTimer timer;timer.start();while(!condition() && timer.elapsed()<timeout)QCoreApplication::processEvents(QEventLoop::AllEvents,10);
    return condition();
}
static QByteArray packet(quint8 kind,const QByteArray &payload) {
    QByteArray value(5,0);value[0]=char(kind);qToBigEndian<quint32>(payload.size(),value.data()+1);return value+payload;
}
static QByteArray context(quint8 flags,const QStringList &fields) {
    QByteArray result(2,0);result[0]=1;result[1]=char(flags);
    for(const auto &field:fields){const auto bytes=field.toUtf8();QByteArray length(2,0);qToBigEndian<quint16>(bytes.size(),length.data());result+=length+bytes;}
    return packet(10,result);
}
struct Server {
    QLocalServer listener;QHash<QByteArray,QLocalSocket *> sockets;QHash<QLocalSocket *,QByteArray> received;int accepts=0;bool legacy=false;
    explicit Server(const QString &path,bool old=false):legacy(old) {
        require(listener.listen(path),"private metadata socket");
        QObject::connect(&listener,&QLocalServer::newConnection,&listener,[this]{
            while(listener.hasPendingConnections()) {
                auto *socket=listener.nextPendingConnection();++accepts;
                QObject::connect(socket,&QLocalSocket::readyRead,&listener,[this,socket]{
                    received[socket]+=socket->readAll();
                    if(legacy && received[socket]=="WAT3\n")socket->disconnectFromServer();
                    else if(received[socket].endsWith('\n'))sockets[received[socket]]=socket;
                });
            }
        });
    }
    bool ready(){return sockets.contains("CTX1\n") && sockets.contains(legacy?"WAT2\n":"WAT3\n");}
    void send(const QByteArray &key,const QByteArray &bytes){auto *s=sockets.value(key);require(s,"subscription ready");s->write(bytes);s->flush();}
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QTemporaryDir temp;require(temp.isValid(),"temporary root");
    const auto path=temp.filePath("runtime.ctl");Server server(path);RuntimeFacts facts(path);int changes=0;
    QObject::connect(&facts,&RuntimeFacts::changed,&app,[&]{++changes;});
    require(wait([&]{return server.ready();}),"independent read-only CTX1 and WAT2 handshakes");
    require(server.accepts==2,"only metadata subscriptions, no terminal attach");
    const auto cwd=temp.filePath("café <folder>\nsecond");
    const auto frame=context(1,{cwd,"repo","feature/test","worktree"});
    // Exercise every frame boundary, including Unicode byte boundaries.
    for(char byte:frame){server.send("CTX1\n",QByteArray(1,byte));QCoreApplication::processEvents();}
    server.send("WAT3\n",packet(6,QByteArray::fromHex("0101")));
    require(wait([&]{return facts.facts()["agent"].toString()=="Codex" && facts.facts()["context_available"].toBool();}),"fragmented context and agent frames");
    require(facts.facts()["title"].toString()=="repo · feature/test · worktree","Git/worktree caption");
    require(!facts.facts()["cwd"].toString().contains('\n'),"control-safe display text");
    const int before=changes;
    server.send("CTX1\n",frame+frame);server.send("WAT3\n",packet(6,QByteArray::fromHex("0101")));
    QElapsedTimer settle;settle.start();while(settle.elapsed()<40)QCoreApplication::processEvents();
    require(changes==before,"identical metadata does not invalidate UI");
    server.send("WAT3\n",packet(6,QByteArray::fromHex("0102"))+packet(6,QByteArray::fromHex("0103")));
    require(wait([&]{return facts.facts()["state"].toString()=="Ready";}),"coalesced agent transitions");
    server.send("WAT3\n",packet(6,QByteArray::fromHex("0602")));
    require(wait([&]{return facts.facts()["needs_input"].toBool();}),"needs input is authoritative runtime state");
    server.send("CTX1\n",context(5,{cwd,"spoof","branch","tree"}));
    require(wait([&]{return facts.facts()["remote"].toBool();}),"remote unavailable");
    require(facts.facts()["cwd"].toString().isEmpty() && facts.facts()["title"]=="Remote session","remote never exposes local cwd fallback");
    server.send("CTX1\n",packet(10,QByteArray::fromHex("01010001ff000000000000")));
    require(wait([&]{return !facts.facts()["remote"].toBool();}),"malformed UTF8 clears only context");
    require(facts.facts()["agent"]=="Copilot" && facts.facts()["needs_input"].toBool(),"metadata failure isolates other feed");
    server.send("WAT3\n",packet(6,QByteArray::fromHex("0801")));
    require(wait([&]{return !facts.facts()["agent_available"].toBool();}),"unknown wire tag rejected");
    require(facts.facts()["agent"].toString().isEmpty() && facts.facts()["state"].toString().isEmpty(),"stale agent labels cleared");
    facts.refresh();require(wait([&]{return server.accepts==4;}),"explicit refresh reconnects failed subscriptions");
    // Additional independent malformed-frame cases; no retries/polling after failure.
    const QList<QByteArray> badContexts={packet(9,{}),QByteArray::fromHex("0a00010000"),context(8,{"a","b","c","d"}),context(1,{"a","b","c","d"})+QByteArray::fromHex("0a0000000101")};
    int sequence=0;
    for(const auto &bad:badContexts) {
        const auto other=temp.filePath(QString("bad-%1.ctl").arg(sequence++));Server source(other);RuntimeFacts invalid(other);
        require(wait([&]{return source.ready();}),"malformed test subscriptions");source.send("CTX1\n",bad);
        require(wait([&]{return source.sockets["CTX1\n"]->state()==QLocalSocket::UnconnectedState;}),"bad frame closes bounded feed");
        require(!invalid.facts()["context_available"].toBool(),"malformed stream cannot retain context");
    }
    {
        Server silent(temp.filePath("silent.ctl"));RuntimeFacts idle(temp.filePath("silent.ctl"));
        require(wait([&]{return silent.ready();}),"silent subscriptions connected");
        require(wait([&]{return silent.sockets["CTX1\n"]->state()==QLocalSocket::UnconnectedState && silent.sockets["WAT3\n"]->state()==QLocalSocket::UnconnectedState;}),"bounded initial reply deadline");
        require(silent.accepts==2,"no reconnect timer");
    }
    {
        Server old(temp.filePath("old.ctl"),true);RuntimeFacts compatible(temp.filePath("old.ctl"));
        require(wait([&]{return old.ready();}),"old runtime accepts baseline fallback");
        require(old.accepts==3,"one bounded fallback without touching context");
        old.send("WAT2\n",packet(6,QByteArray::fromHex("0603")));
        require(wait([&]{return compatible.facts()["agent"].toString()=="Copilot";}),"old lifecycle feed remains useful");
    }
    {
        Server pi(temp.filePath("pi.ctl"));RuntimeFacts extended(temp.filePath("pi.ctl"));
        require(wait([&]{return pi.ready();}),"Pi metadata subscription");
        pi.send("WAT3\n",packet(6,QByteArray::fromHex("0701")));
        require(wait([&]{return extended.facts()["agent"].toString()=="Pi" && extended.facts()["state"].toString()=="Working";}),"Pi extended identity and lifecycle");
    }
    {
        Server partial(temp.filePath("partial.ctl"));RuntimeFacts invalid(temp.filePath("partial.ctl"));
        require(wait([&]{return partial.ready();}),"partial extended subscription");
        partial.send("WAT3\n",QByteArray(1,char(6)));partial.sockets["WAT3\n"]->disconnectFromServer();
        QElapsedTimer drain;drain.start();while(drain.elapsed()<80)QCoreApplication::processEvents();
        require(partial.accepts==2 && !invalid.facts()["agent_available"].toBool(),"partial close cannot trigger baseline fallback");
    }
    const auto alias=temp.filePath("alias.ctl");require(QFile::link(path,alias),"symlink fixture");RuntimeFacts linked(alias);
    QCoreApplication::processEvents();require(server.accepts==4,"symlink metadata endpoint rejected");
    std::puts("PASS: read-only bounded metadata feeds, fragments/coalescing, cached change notifications, Git/worktree labels, authoritative agent state, remote masking, malformed/timeout isolation, explicit refresh");
}
