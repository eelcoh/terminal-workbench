#include "ui_hosts.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
#include <functional>
static void require(bool result,const char *why) {if(!result){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static bool wait(const std::function<bool()> &condition,int timeout=4000) {
    QElapsedTimer timer;timer.start();while(!condition() && timer.elapsed()<timeout)QCoreApplication::processEvents(QEventLoop::AllEvents,20);
    return condition();
}
static void write(const QString &path,const QByteArray &bytes) {QFile file(path);require(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(),"fixture write");}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    for(const auto *good:{"build","user@box.local","10.0.0.2","[::1]","host%eth0"})require(UiHosts::validDestination(good),"valid destination");
    for(const auto *bad:{"","-oProxyCommand=x","a b","a;b","$(x)","a\nb"})require(!UiHosts::validDestination(bad),"invalid destination rejected");
    require(UiHosts::aliases("Host build lab # work\n  HostName 10.0.0.2\nHost *.internal !skip\nhost=box\nMatch host x\nHost build\n")==QStringList({"build","lab","box"}),"Host aliases without patterns or duplicates");

    QTemporaryDir temp;require(temp.isValid(),"temporary root");
    const auto path=temp.filePath("ui/hosts.json"),ssh=temp.filePath("ssh_config");
    write(ssh,"Host alpha beta\nHost *\n");
    UiHosts hosts(path,ssh);
    require(hosts.saved().isEmpty() && hosts.error().isEmpty() && hosts.suggestions()==QStringList({"alpha","beta"}),"empty list and ssh suggestions");
    const QVariantList configured{QVariantMap{{"id","build"},{"name","Build box"},{"destination","build"}},QVariantMap{{"id","lab"},{"name","Lab"},{"destination","lab"}}};
    require(hosts.freeId("Build box",configured)=="build-box" && hosts.freeId("build",configured)=="build-2" && hosts.freeId("!!!",configured)=="host","free IDs from names");
    require(hosts.save({{"id","lab"},{"name","My lab"},{"destination","me@lab"},{"directory","~/src"}}),"save a host");
    require(QFile::exists(path) && QJsonDocument::fromJson([&]{QFile f(path);return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();}())["version"].toInt()==1,"saved file");
    const auto merged=hosts.merge(configured);
    require(merged.size()==2 && merged[0].toMap()["id"]=="build" && merged[1].toMap()["destination"]=="me@lab" && merged[1].toMap()["saved"].toBool(),"Band's entry wins for the same ID");
    require(!hosts.save({{"id","x"},{"destination","-oProxyCommand=x"}}) && !hosts.error().isEmpty() && hosts.saved().size()==1,"invalid host refused, list kept");
    UiHosts other(path,{});
    require(other.saved().size()==1 && other.saved()[0].toMap()["name"]=="My lab","another window reads the saved list");
    require(hosts.save({{"id","alpha"},{"destination","alpha"}}),"second host");
    require(wait([&]{return other.saved().size()==2;}),"another window follows changes");
    require(hosts.remove("lab") && hosts.saved().size()==1 && !hosts.remove("missing"),"remove a saved host");
    write(path,"{\"version\":1,\"hosts\":[{\"id\":\"ok\",\"destination\":\"ok\"},{\"id\":\"bad id\",\"destination\":\"x\"}]}");
    require(wait([&]{return hosts.saved().size()==1 && hosts.saved()[0].toMap()["id"]=="ok";}) && !hosts.error().isEmpty(),"invalid saved entries skipped with a notice");
    // Reachability: ssh -G resolves; a TCP connect decides; proxied hosts stay unknown.
    const auto plainTarget=UiHosts::resolved("hostname 10.0.0.2\nport 2222\nproxyjump none\nproxycommand none\n");
    require(plainTarget["host"]=="10.0.0.2" && plainTarget["port"].toInt()==2222 && !plainTarget["proxied"].toBool(),"ssh -G address and port");
    require(UiHosts::resolved("hostname x\nproxyjump bastion\n")["proxied"].toBool() && UiHosts::resolved("hostname x\nproxycommand nc %h %p\n")["proxied"].toBool(),"proxied hosts recognised");
    QTcpServer listener;require(listener.listen(QHostAddress::LocalHost),"local listener");
    QTcpServer closed;require(closed.listen(QHostAddress::LocalHost),"port to close");const auto closedPort=closed.serverPort();closed.close();
    const auto fake=temp.filePath("ssh");
    write(fake,QString("#!/bin/sh\n[ \"$1\" = -G ] || exit 1\ncase \"$3\" in up) printf 'hostname 127.0.0.1\\nport %1\\n';; down) printf 'hostname 127.0.0.1\\nport %2\\n';; jump) printf 'hostname x\\nproxyjump bastion\\n';; *) exit 255;; esac\n").arg(listener.serverPort()).arg(closedPort).toUtf8());
    QFile::setPermissions(fake,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
    UiHosts reach({},{},nullptr,fake);
    reach.check({QVariantMap{{"id","a"},{"destination","up"}},QVariantMap{{"id","b"},{"destination","down"}},QVariantMap{{"id","c"},{"destination","jump"}},QVariantMap{{"id","d"},{"destination","broken"}}});
    require(wait([&]{const auto r=reach.reachability();return r["a"]=="reachable" && r["b"]=="unreachable" && r["c"]=="unknown" && r["d"]=="unknown";}),"reachable, unreachable, proxied and failed resolution");
    std::puts("PASS: destinations, ssh Host aliases, free IDs, save/merge/remove, refusal, other windows, invalid entries, reachability");
}
