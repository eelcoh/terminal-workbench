#include "ui_themes.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
void check(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
void put(const QString &path,const QByteArray &data) {QFile file(path);check(file.open(QIODevice::WriteOnly),"open fixture");check(file.write(data)==data.size(),"write fixture");}
QByteArray get(const QString &path) {QFile file(path);check(file.open(QIODevice::ReadOnly),"read fixture");return file.readAll();}
QByteArray palette(int number=0) {return "# synthetic import fixture\nbackground = #132435\nforeground = #dceeff\npalette = 1=#a1b2c3\nconfig-file = /never/execute\n# variant "+QByteArray::number(number)+'\n';}
struct Response {QByteArray data;QNetworkReply::NetworkError error=QNetworkReply::NoError;int status=200;bool hang=false,redirect=false;};
class Reply final : public QNetworkReply {
public:
    Reply(const QNetworkRequest &request,const Response &response,int *aborted,QObject *parent)
        :QNetworkReply(parent),data_(response.data),aborted_(aborted) {
        setRequest(request);setUrl(request.url());setOperation(QNetworkAccessManager::GetOperation);open(QIODevice::ReadOnly|QIODevice::Unbuffered);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,response.status);
        if(response.redirect)setAttribute(QNetworkRequest::RedirectionTargetAttribute,QUrl("https://elsewhere.invalid/theme"));
        if(response.error!=NoError)setError(response.error,"Synthetic network failure");
        if(!response.hang)QTimer::singleShot(0,this,[this]{ready_=true;setFinished(true);Q_EMIT readyRead();Q_EMIT finished();});
    }
    void abort() override {++*aborted_;if(!isFinished()) {setError(OperationCanceledError,"Cancelled");setFinished(true);Q_EMIT finished();}}
    qint64 bytesAvailable() const override {return (ready_?data_.size()-offset_:0)+QNetworkReply::bytesAvailable();}
protected:
    qint64 readData(char *destination,qint64 maximum) override {
        if(!ready_)return 0;
        const auto size=qMin(maximum,qint64(data_.size()-offset_));
        if(!size)return -1;
        std::memcpy(destination,data_.constData()+offset_,size);offset_+=size;return size;
    }
private:
    QByteArray data_;qint64 offset_=0;bool ready_=false;int *aborted_;
};
class Network final : public QNetworkAccessManager {
public:
    Response next;QStringList requested;int aborted=0;
protected:
    QNetworkReply *createRequest(Operation operation,const QNetworkRequest &request,QIODevice *outgoing) override {
        Q_UNUSED(operation);Q_UNUSED(outgoing);requested.append(request.url().toString());return new Reply(request,next,&aborted,this);
    }
};
void wait(UiThemes &themes,int milliseconds=1500) {
    if(!themes.busy())return;
    QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);deadline.setInterval(milliseconds);
    QObject::connect(&themes,&UiThemes::changed,&loop,[&]{if(!themes.busy())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);deadline.start();loop.exec();
    check(!themes.busy(),"theme operation did not finish");
}
void modelChecks(const QString &base) {
    const auto store=base+"/saved";const auto file=base+"/café #100%.ghostty";put(file,palette());
    Network network;UiThemes first(store,nullptr,&network);check(network.requested.isEmpty(),"startup fetched a theme");
    const int builtins=first.catalog().size();check(builtins==workbench_theme_builtin_count(),"missing built-ins");
    int selected=-1;QObject::connect(&first,&UiThemes::imported,&first,[&](int i){selected=i;});
    first.importTheme(file,"My theme","Fixture Author · MIT");check(first.error().isEmpty() && selected>=builtins,"local import failed");
    const auto item=first.theme(selected);check(item["background"]=="#132435" && item["ignored"].toInt()==1,"parsed colors or ignored setting count");
    check(item["source"].toString().contains("caf") && item["attribution"]=="Fixture Author · MIT","source metadata lost");
    WorkbenchTheme color;check(workbench_theme_get(selected,&color) && color.palette[1].r==0xa1,"renderer registry differs from imported theme");
    const auto manifest=get(store+"/imports.json");const auto count=first.catalog().size();
    first.importTheme(file,"Duplicate name");check(first.catalog().size()==count && get(store+"/imports.json")==manifest,"duplicate changed catalog");
    UiThemes offline(store,nullptr,&network);check(offline.error().isEmpty() && offline.catalog().size()==count && network.requested.isEmpty(),"offline reload fetched/lost import");
    // A stale second window must merge, not overwrite imports saved by the first.
    const auto other=base+"/other.conf";put(other,palette(2));offline.importTheme(other,"Second window");check(offline.error().isEmpty(),"second-window import");
    const auto third=base+"/third.conf";put(third,palette(3));first.importTheme(third,"First window again");
    UiThemes merged(store,nullptr,&network);check(merged.catalog().size()==builtins+3,"stale window overwrote a foreign import");
    const auto stable=get(store+"/imports.json");
    for(const auto &bytes:QList<QByteArray>{QByteArray("background=#zzzzzz\n"),QByteArray("command=touch /never\n"),QByteArray("background=#123456\n\0bad",24),QByteArray("background=#123456\n#")+QByteArray(1,char(0xff)),QByteArray(ThemeStore::maxBytes+1,'x')}) {
        const auto bad=base+"/bad.conf";put(bad,bytes);const int before=first.catalog().size();first.importTheme(bad);
        check(!first.error().isEmpty() && first.catalog().size()==before && get(store+"/imports.json")==stable,"invalid import changed saved/current catalog");
    }
    QLockFile lock(store+"/import.lock");check(lock.tryLock(),"fixture lock");
    put(third,palette(4));first.importTheme(third);check(!first.error().isEmpty() && get(store+"/imports.json")==stable,"import ignored another writer");lock.unlock();
    // Unknown/corrupt saved data remains untouched and keeps built-in palettes usable.
    put(store+"/imports.json","{\"version\":999,\"imports\":[]}");const auto corrupt=get(store+"/imports.json");
    UiThemes broken(store,nullptr,&network);check(!broken.error().isEmpty() && broken.catalog().size()==builtins,"bad store replaced built-ins");
    broken.importTheme(file);check(get(store+"/imports.json")==corrupt,"bad store was overwritten");
    put(store+"/imports.json",stable);const auto hash=ThemeStore::digest(palette());put(store+'/'+hash+".ghostty","tampered");
    UiThemes checksum(store,nullptr,&network);check(!checksum.error().isEmpty() && checksum.catalog().size()==builtins,"bad checksum accepted");
}
void networkChecks(const QString &base) {
    const QString page="https://terminalcolors.com/themes/nord/default/";
    check(UiThemes::downloadUrl(page).toString()=="https://terminalcolors.com/downloads/ghostty/nord-default","page URL mapping");
    for(const auto &url:QStringList{"http://terminalcolors.com/themes/nord/default/","https://terminalcolors.com.evil.test/downloads/ghostty/a","https://user:secret@terminalcolors.com/downloads/ghostty/a","https://terminalcolors.com:123/downloads/ghostty/a","https://terminalcolors.com/downloads/iterm/a","https://terminalcolors.com/downloads/ghostty/a?x=1"})check(UiThemes::downloadUrl(url).isEmpty(),"unsafe/unsupported URL accepted");
    Network network;UiThemes themes(base+"/network",nullptr,&network,40);const int builtins=themes.catalog().size();
    network.next={palette(10)};themes.importTheme(page);check(themes.busy(),"download was synchronous");wait(themes);
    check(themes.error().isEmpty() && themes.catalog().size()==builtins+1 && network.requested.last().endsWith("/ghostty/nord-default"),"network import failed");
    check(themes.catalog().last().toMap()["name"]=="Nord Default","page-derived theme name");
    const auto manifest=get(base+"/network/imports.json");
    network.next={palette(11),QNetworkReply::ConnectionRefusedError};themes.importTheme(page);wait(themes);check(!themes.error().isEmpty(),"network failure accepted");
    network.next={palette(12),QNetworkReply::NoError,302,false,true};themes.importTheme(page);wait(themes);check(themes.error().contains("redirected"),"redirect followed");
    network.next={QByteArray(ThemeStore::maxBytes+1,'x')};themes.importTheme(page);wait(themes);check(themes.error().contains("64 KiB"),"oversized body accepted");
    network.next={{},QNetworkReply::NoError,200,true};themes.importTheme(page);themes.cancelImport();QCoreApplication::processEvents();
    check(!themes.busy() && network.aborted>0,"cancellation retained a download");
    themes.importTheme(page);wait(themes);check(themes.error().contains("timed out"),"deadline did not end download");
    check(themes.catalog().size()==builtins+1 && get(base+"/network/imports.json")==manifest,"failed/cancelled network request changed store");
    const int requests=network.requested.size();themes.importTheme("https://unrelated.invalid/theme");
    check(network.requested.size()==requests && !themes.error().isEmpty(),"unsupported host was requested");
}
void limits(const QString &base) {
    QVector<StoredTheme> records;QString error;
    for(int i=0;i<ThemeStore::maxThemes;++i) {const auto data=palette(100+i);records.append({ThemeStore::digest(data),"Fixture "+QString::number(i),"file:///fixture",{},data});}
    check(ThemeStore::save(base+"/limit",records,error),"save capacity fixture");
    UiThemes themes(base+"/limit");check(themes.catalog().size()==workbench_theme_builtin_count()+ThemeStore::maxThemes,"capacity reload");
    const auto file=base+"/over.conf";put(file,palette(999));const auto before=get(base+"/limit/imports.json");themes.importTheme(file);
    check(themes.error().contains("64") && get(base+"/limit/imports.json")==before,"capacity changed saved themes");
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir directory;check(directory.isValid(),"temporary directory");
        modelChecks(directory.path());networkChecks(directory.path());limits(directory.path());
        if(argc==3 && QString::fromLocal8Bit(argv[1])=="--live-file") {
            UiThemes live(directory.path()+"/live");live.importTheme(QString::fromLocal8Bit(argv[2]),"Live export verification");check(live.error().isEmpty(),"live export parse");
            std::puts("PASS: supplied live Ghostty export parsed and cached in disposable storage");
        }
        if(argc==3 && QString::fromLocal8Bit(argv[1])=="--live-url") {
            UiThemes live(directory.path()+"/live-network");live.importTheme(QString::fromLocal8Bit(argv[2]),"Live TerminalColors verification");wait(live,20000);
            if(!live.error().isEmpty())throw std::runtime_error(live.error().toStdString());
            check(live.catalog().size()==workbench_theme_builtin_count()+1,"live link did not add a palette");
            std::puts("PASS: live TerminalColors link through Qt HTTPS, Ghostty parsing and disposable offline storage");
        }
        std::puts("PASS: bounded Ghostty imports, offline metadata/checksums, duplicates, stale-window merge, writer isolation, corruption preservation, URL scope, download failures/redirects/limits/cancel/deadline");return 0;
    } catch(const std::exception &error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
