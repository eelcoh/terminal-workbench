#include "ui_build_info.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <cstdio>
#include <cstdlib>

static void require(bool condition,const char *message)
{
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}

int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);
    QJsonObject config{{"build_info",QJsonObject{{"kind","installed"},{"version","0.1.0"},{"revision",QString(40,'a')},
        {"source_dirty",true},{"payload_root","/installed preview"},{"python","/python"},
        {"secret","do not copy"}}},{"runtime","/runtime"},{"viewer","/viewer"},
        {"preferences","/config/config.conf"},{"state","/state"},{"view_state","/view.json"},
        {"catalog",QJsonObject{{"name","private project"}}},{"terminal_output","secret output"}};
    UiBuildInfo info(config);
    require(info.summary().startsWith("0.1.0 · aaaaaaaaaaaa") && info.summary().contains("local changes"),"version/revision/dirty summary");
    const auto bytes=info.diagnostics().toUtf8();const auto report=QJsonDocument::fromJson(bytes).object();
    require(report["configured_runtime"].toString()=="/runtime" && report["payload_root"].toString()=="/installed preview","client paths");
    require(report["qt_version"].toString()==QString::fromLatin1(qVersion()),"actual Qt version");
    require(!bytes.contains("private project") && !bytes.contains("secret output") && !bytes.contains("do not copy"),"explicit diagnostic allowlist");
    info.copyDiagnostics();require(QGuiApplication::clipboard()->text()==info.diagnostics(),"copy exact diagnostics");
    UiBuildInfo checkout(QJsonObject{});require(checkout.summary()=="development checkout","checkout fallback");
    std::puts("PASS: client build identity, allowlisted diagnostics and explicit clipboard copy");
}
