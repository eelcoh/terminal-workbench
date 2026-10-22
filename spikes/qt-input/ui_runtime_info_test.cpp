#include "ui_runtime_info.h"
#include <QClipboard>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>

static void require(bool ok,const char *why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static bool wait(const std::function<bool()> &condition,int timeout=1000){QElapsedTimer timer;timer.start();while(!condition() && timer.elapsed()<timeout)QCoreApplication::processEvents(QEventLoop::AllEvents,10);return condition();}
struct Server {
    QLocalServer listener;int accepts=0;QByteArray commands;QPointer<QLocalSocket> last;
    std::function<void(QLocalSocket *)> reply;
    explicit Server(const QString &path){
        require(listener.listen(path),"listen on owned private endpoint");
        QObject::connect(&listener,&QLocalServer::newConnection,&listener,[this]{
            while(listener.hasPendingConnections()){
                auto *socket=listener.nextPendingConnection();last=socket;++accepts;
                auto bytes=std::make_shared<QByteArray>();
                QObject::connect(socket,&QLocalSocket::readyRead,&listener,[this,socket,bytes]{
                    *bytes+=socket->readAll();if(!bytes->endsWith('\n'))return;
                    commands+=*bytes;require(*bytes=="CAPS\n","inspection sent only CAPS, without PTY/control mutation");bytes->clear();
                    if(reply)reply(socket);
                });
            }
        });
    }
};
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);QTemporaryDir root;require(root.isValid(),"private root");
    Server server(root.filePath("pane.ctl"));UiRuntimeInfo info(root.path(),"/current/runtime",nullptr,80);
    require(info.report().isEmpty() && server.accepts==0,"construction performs no runtime access");
    const QByteArray valid=R"({"protocol":1,"runtime_pid":42,"capabilities":["snapshot","agent-watch-v3","future.feature"],"origin":{"version":1,"runtime_binary":"/older/runtime","agent_bin":"/older/helpers café","secret":"do not copy"},"terminal_output":"secret output","environment":{"TOKEN":"secret token"}})";
    server.reply=[&](QLocalSocket *socket){socket->write(valid.left(19));socket->flush();QTimer::singleShot(10,socket,[socket,valid]{socket->write(valid.mid(19)+'\n');socket->flush();});};
    info.inspect("pane");require(info.report()["state"]=="checking","explicit loading state");
    require(wait([&]{return info.report()["state"]=="available";}),"fragmented CAPS reply");
    require(info.report()["identity"]=="pane" && info.report()["runtime_pid"]==42 && info.report()["agent_bin"]==QString::fromUtf8("/older/helpers café"),"stable target and immutable launch origins");
    require(info.report()["capabilities"].toStringList().contains("future.feature"),"unknown bounded capabilities remain optional");
    const auto bytes=info.diagnostics().toUtf8();const auto diagnostic=QJsonDocument::fromJson(bytes).object();
    require(diagnostic["configured_runtime"]=="/current/runtime" && diagnostic["runtime_binary"]=="/older/runtime","configured and launched paths remain distinct without claiming build equivalence");
    require(!bytes.contains("secret") && !bytes.contains("terminal_output") && !bytes.contains("environment"),"copy schema excludes arbitrary response fields and payloads");
    require(info.copyDiagnostics() && QGuiApplication::clipboard()->text()==info.diagnostics(),"explicit exact diagnostic copy");
    server.reply=[](QLocalSocket *socket){socket->write("{\"protocol\":1,\"capabilities\":[\"snapshot\"]}\n");socket->flush();};
    info.inspect("pane");require(wait([&]{return info.report()["state"]=="available";}),"older CAPS without origin or PID remains useful");
    require(!info.report()["origin_available"].toBool() && info.report()["runtime_binary"].toString().isEmpty(),"old origins clear, rather than leaking prior target paths");
    const QList<QByteArray> invalid={"{}\n","[]\n","{\"protocol\":2,\"capabilities\":[]}\n","{\"protocol\":1,\"capabilities\":[3]}\n","{\"protocol\":1,\"capabilities\":[],\"runtime_pid\":0}\n","{\"protocol\":1,\"capabilities\":[],\"runtime_pid\":1.5}\n",QByteArray(32769,'x')};
    for(const auto &reply:invalid){
        server.reply=[reply](QLocalSocket *socket){socket->write(reply);socket->flush();};
        info.inspect("pane");require(wait([&]{return info.report()["state"]=="unavailable";}),"malformed/oversized response is unavailable");
        require(!info.report().contains("runtime_binary") && !info.report().contains("capabilities"),"failed inspection has no stale evidence");
    }
    for(const auto &origin:{QByteArray("{\"version\":2,\"runtime_binary\":\"/future\"}"),QByteArray("{\"version\":1,\"runtime_binary\":\"relative\"}")}){
        server.reply=[origin](QLocalSocket *socket){socket->write("{\"protocol\":1,\"capabilities\":[],\"origin\":"+origin+"}\n");socket->flush();};
        info.inspect("pane");require(wait([&]{return info.report()["state"]=="available";}),"invalid/future optional origins do not discard capabilities");require(!info.report()["origin_available"].toBool(),"invalid origin is not treated as a path");
    }
    server.reply=[](QLocalSocket *socket){socket->disconnectFromServer();};
    info.inspect("pane");require(wait([&]{return info.report()["state"]=="unavailable";}),"old runtime empty close has no fallback request");
    server.reply=[](QLocalSocket *){};const auto before=server.accepts;
    info.inspect("pane");require(wait([&]{return info.report()["state"]=="unavailable";}),"silent runtime has bounded deadline");
    require(server.accepts==before+1,"timeout does not reconnect or poll");
    info.inspect("pane");require(wait([&]{return server.accepts==before+2;}),"new explicit inspection");
    require(!info.copyDiagnostics(),"incomplete report cannot replace clipboard");
    info.clear();require(info.report().isEmpty() && info.diagnostics().isEmpty(),"closing cancels and clears the target");
    require(wait([&]{return server.last && server.last->state()==QLocalSocket::UnconnectedState;}),"closing disconnects the owned inspection");
    info.inspect("../pane");require(info.report().isEmpty() && server.accepts==before+2,"unsafe identity cannot create an endpoint request");
    info.inspect("missing");require(info.report()["state"]=="unavailable","absent endpoint stays local and starts no service");
    QFile file(root.filePath("regular.ctl"));require(file.open(QIODevice::WriteOnly),"regular-file fixture");file.write("preserved");file.close();
    info.inspect("regular");require(info.report()["state"]=="unavailable","regular file refused");
    require(QFile::link(root.filePath("pane.ctl"),root.filePath("link.ctl")),"symlink fixture");
    info.inspect("link");require(info.report()["state"]=="unavailable" && server.accepts==before+2,"symlink cannot redirect an inspection");
    // Switch targets while a response is outstanding. Its later signal cannot
    // replace the new target or retain old origin data.
    info.inspect("pane");require(wait([&]{return server.accepts==before+3;}),"old pending inspection");
    info.inspect("missing");QElapsedTimer settle;settle.start();while(settle.elapsed()<120)QCoreApplication::processEvents();
    require(info.report()["identity"]=="missing" && info.report()["state"]=="unavailable","stale signals remain scoped to their request");
    std::puts("PASS: bounded read-only CAPS inspection, legacy/optional evidence, fragmentation, malformed/oversized/timeout isolation, stable target cancellation, socket ownership and allowlisted clipboard diagnostics");
}
