#include "ui_lifecycle.h"
#include <QCoreApplication>
#include <QLocalServer>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <functional>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>
static void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static bool wait(const std::function<bool()> &condition){QElapsedTimer t;t.start();while(!condition() && t.elapsed()<2000)QCoreApplication::processEvents(QEventLoop::AllEvents,5);return condition();}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir root;const auto endpoint=root.filePath("service.rpc");
    QLocalServer server;require(server.listen(endpoint),"private server");
    QList<QLocalSocket *> peers;QList<QJsonObject> requests;QHash<QLocalSocket *,QByteArray> buffers;
    QObject::connect(&server,&QLocalServer::newConnection,&app,[&]{while(server.hasPendingConnections()){
        auto *socket=server.nextPendingConnection();peers.append(socket);
        QObject::connect(socket,&QLocalSocket::readyRead,&app,[&,socket]{buffers[socket]+=socket->readAll();if(buffers[socket].contains('\n'))requests.append(QJsonDocument::fromJson(buffers[socket]).object());});
    }});
    UiLifecycle client(endpoint,nullptr,100);
    auto request=[&]{const int next=requests.size()+1;client.inspect("one");require(wait([&]{return requests.size()==next;}),"readonly request delivered");};
    QByteArray good="{\"protocol\":1,\"ok\":true,\"id\":\"one\",\"state\":\"stopped\",\"lifecycle\":{\"reason\":\"exited\",\"message\":\"Shell exited with code 42.\"}}\n";
    request();client.inspect("one");require(peers.size()==1,"duplicate probes coalesced");
    require(requests.last()==QJsonObject{{"protocol",1},{"method","terminal.lifecycle"},{"identity","one"}},"no startup/mutation fields");
    peers.last()->write(good.left(8));peers.last()->flush();QCoreApplication::processEvents();
    require(client.values()["one"].toMap()["busy"].toBool(),"fragment remains pending");
    peers.last()->write(good.mid(8));peers.last()->flush();require(wait([&]{return !client.values()["one"].toMap()["busy"].toBool();}),"fragment complete");
    require(client.values()["one"].toMap()["message"]=="Shell exited with code 42.","recorded exit reason");
    QList<QByteArray> invalid={QByteArray(good).replace("\"id\":\"one\"","\"id\":\"other\""),"not-json\n",QByteArray(8193,'x'),"{\"protocol\":1,\"ok\":true}\n"};
    for(const auto &bytes:invalid){request();peers.last()->write(bytes);peers.last()->flush();require(wait([&]{return !client.values()["one"].toMap()["busy"].toBool();}),"invalid reply bounded");require(client.values()["one"].toMap()["state"]=="unavailable","invalid reply cannot become stopped/running");}
    request();peers.last()->disconnectFromServer();require(wait([&]{return !client.values()["one"].toMap()["busy"].toBool();}),"EOF handled");
    request();require(wait([&]{return !client.values()["one"].toMap()["busy"].toBool();}),"deadline bounded");
    request();client.retain({});require(client.values().isEmpty(),"retirement clears pending cache");QCoreApplication::processEvents();require(client.values().isEmpty(),"late failure cannot recreate removed identity");
    UiLifecycle missing(root.filePath("missing.rpc"));missing.inspect("two");require(wait([&]{return !missing.values()["two"].toMap()["busy"].toBool();}),"missing service handled");require(!QFileInfo::exists(root.filePath("missing.rpc")),"readonly lookup never starts service");
    std::puts("PASS: lifecycle-only requests, deduplication, fragmented/invalid/oversized/EOF/deadline isolation, missing-service non-start and retired observer safety");
}
