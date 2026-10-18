#include "ui_catalog_feed.h"
#include <QCoreApplication>
#include <QLocalServer>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSaveFile>
#include <QFile>
#include <QJsonArray>
#include <cstdio>
#include <cstdlib>
#include <functional>
static void check(bool ok,const char *message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static bool wait(const std::function<bool()> &condition){QElapsedTimer t;t.start();while(!condition() && t.elapsed()<2500)QCoreApplication::processEvents(QEventLoop::AllEvents,5);return condition();}
static void settle(){QElapsedTimer t;t.start();while(t.elapsed()<60)QCoreApplication::processEvents(QEventLoop::AllEvents,5);}
static void put(const QString &path,const QByteArray &text){QSaveFile file(path);check(file.open(QIODevice::WriteOnly) && file.write(text)==text.size() && file.commit(),"atomic catalog replacement");}
static QJsonObject catalog(const QString &name){return {{"version",1},{"workspaces",QJsonArray{QJsonObject{{"id","w"},{"name",name},{"terminals",QJsonArray{QJsonObject{{"id","t"},{"name","Shell"}}}}}}}};}
static QByteArray reply(const QJsonObject &data){return QJsonDocument(QJsonObject{{"protocol",1},{"ok",true},{"data",data}}).toJson(QJsonDocument::Compact)+'\n';}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir root;check(root.isValid(),"private directory");const auto path=root.filePath("workspaces.json"),endpoint=root.filePath("service.rpc");put(path,"initial");
    QLocalServer server;check(server.listen(endpoint),"fake service");QList<QLocalSocket *> peers;QList<QJsonObject> requests;QHash<QLocalSocket *,QByteArray> buffers;
    QObject::connect(&server,&QLocalServer::newConnection,&app,[&]{while(server.hasPendingConnections()){
        auto *peer=server.nextPendingConnection();peers.append(peer);
        QObject::connect(peer,&QLocalSocket::readyRead,&app,[&,peer]{buffers[peer]+=peer->readAll();if(buffers[peer].contains('\n')){requests.append(QJsonDocument::fromJson(buffers[peer]).object());buffers[peer].clear();}});
    }});
    const auto first=catalog("First"),second=catalog("Second"),third=catalog("Third");
    UiCatalogFeed feed(root.path(),first.toVariantMap(),nullptr,200,10);QList<QVariantMap> updates;QObject::connect(&feed,&UiCatalogFeed::updated,&app,[&](const QVariantMap &value){updates.append(value);});
    auto awaitRequest=[&](int count){check(wait([&]{return requests.size()==count;}),"catalog request");check(requests.last()==QJsonObject{{"protocol",1},{"method","catalog.read"}},"read-only method has no startup or mutation fields");};
    auto answer=[&](const QJsonObject &data){peers.last()->write(reply(data));peers.last()->flush();settle();};
    awaitRequest(1);const auto encoded=reply(first);peers.last()->write(encoded.left(7));peers.last()->flush();settle();check(updates.isEmpty(),"fragment is not a snapshot");peers.last()->write(encoded.mid(7));peers.last()->flush();settle();check(updates.isEmpty() && feed.error().isEmpty(),"unchanged snapshot deduped");
    put(path,"renamed atomically");awaitRequest(2);answer(second);check(updates.size()==1 && updates.last()==second.toVariantMap(),"filesystem update published");
    put(path,"replaced again");awaitRequest(3);answer(third);check(updates.size()==2,"file watch rearmed after replacement");
    feed.refresh();awaitRequest(4);peers.last()->write(reply(first));peers.last()->flush();feed.pause();settle();check(updates.size()==2,"late pre-mutation snapshot discarded");
    const int pausedCount=requests.size();put(path,"while mutation is busy");settle();check(requests.size()==pausedCount,"no reads during local mutation");feed.resume(second.toVariantMap());awaitRequest(5);answer(second);check(updates.size()==2,"confirmed mutation seeds deduplication");
    feed.refresh();awaitRequest(6);peers.last()->write("{\"protocol\":1,\"ok\":false,\"error\":\"Unknown session service method\"}\n");peers.last()->flush();check(wait([&]{return !feed.error().isEmpty();}),"old-service fallback notice");put(path,"unsupported server");settle();check(requests.size()==6,"unsupported method does not poll/retry");
    feed.refresh();awaitRequest(7);answer(third);check(feed.error().isEmpty() && updates.size()==3,"explicit retry recovers supported service");
    const QList<QByteArray> bad={"not-json\n",QByteArray(256*1024+1,'x'),"{\"protocol\":1,\"ok\":true,\"data\":{\"version\":1,\"workspaces\":[{}]}}\n"};
    for(const auto &bytes:bad){const int count=requests.size()+1;feed.refresh();awaitRequest(count);peers.last()->write(bytes);peers.last()->flush();check(wait([&]{return !feed.error().isEmpty();}),"malformed snapshot isolated");check(updates.size()==3,"malformed snapshot cannot replace catalog");answer(third);}
    int count=requests.size()+1;feed.refresh();awaitRequest(count);check(wait([&]{return !feed.error().isEmpty();}),"bounded timeout");check(updates.size()==3,"timeout keeps last catalog");
    server.close();feed.refresh();check(wait([&]{return !feed.error().isEmpty();}),"missing service isolated");check(!QFile::exists(root.filePath("service.lock")),"read client never starts service");
    auto invalid=first;invalid["version"]=2;check(!UiCatalogFeed::valid(invalid),"future catalog rejected");
    invalid=first;invalid["workspaces"]=QJsonArray{QJsonObject{{"id","same"},{"name","Good"},{"terminals",QJsonArray{QJsonObject{{"id","same"},{"name","Duplicate"}}}}}};check(!UiCatalogFeed::valid(invalid),"global identities stay unique");
    std::puts("PASS: filesystem catalog invalidation, read-only request, fragmented/bounded replies, atomic rewatch, deduplication, mutation pause, old-service fallback and failure isolation");
}
