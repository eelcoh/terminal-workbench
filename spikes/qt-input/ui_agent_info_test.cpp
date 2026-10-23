#include "ui_agent_info.h"
#include <QGuiApplication>
#include <QClipboard>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char *why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);UiAgentInfo help;
    auto value=help.describe({});require(value["launcher"].toString().isEmpty() && value["agent"].toString().isEmpty(),"unavailable metadata cannot advertise identity or setup");
    value=help.describe({{"agent_available",false},{"agent","Codex"},{"state","Ready"}});
    require(value["launcher"].toString().isEmpty() && value["heading"]!="Ready","stale labels cannot establish ready state");
    value=help.describe({{"agent_available",true},{"agent",""}});require(value["heading"]=="No agent detected","ordinary foreground process is distinct from unavailable feed");
    for(const auto *agent:{"Codex","Claude","Copilot","Pi"}){
        value=help.describe({{"agent_available",true},{"agent",agent},{"state","Status unknown"}});
        require(value["heading"]=="Status unknown" && value["launcher"].toString().startsWith("band-"),"recognition does not establish lifecycle status");
        require(help.copyLauncher(agent) && QGuiApplication::clipboard()->text()==value["launcher"].toString(),"only an allowlisted launcher is copied");
    }
    const auto previous=QGuiApplication::clipboard()->text();
    require(!help.copyLauncher("pi; echo injected") && QGuiApplication::clipboard()->text()==previous,"arbitrary metadata cannot become clipboard commands");
    value=help.describe({{"agent_available",true},{"agent","Gemini"},{"state","Working"}});
    require(value["launcher"].toString().isEmpty() && value["heading"]=="Working","unsupported launcher does not discard valid status");
    value=help.describe({{"agent_available",true},{"agent","Codex"},{"state","invented"}});
    require(value["heading"]=="Status unknown","unknown state retains honest explanation");
    const QStringList ordered={"a","b","c"},waiting={"b","c"};
    require(help.nextWaiting(ordered,waiting,"a",1)=="b" && help.nextWaiting(ordered,waiting,"c",1)=="b","stable-ID next and wrap");
    require(help.nextWaiting(ordered,waiting,"b",-1)=="c","stable-ID reverse wrap");
    require(help.nextWaiting(ordered,{"removed","c"},"a",1)=="c","stale waiting IDs cannot retarget another pane");
    require(help.nextWaiting(ordered,{"removed"},"a",1).isEmpty() && help.nextWaiting({},waiting,"a",1).isEmpty(),"empty and removed queue is a no-op");
    require(help.nextWaiting(ordered,waiting,"missing",-1)=="c","missing selection starts at the last waiting result");
    QStringList oversized;for(int i=0;i<65;++i)oversized.append(QString::number(i));
    require(help.nextWaiting(oversized,{"1"},"0",1).isEmpty(),"bounded client snapshot");
    std::puts("PASS: identity/status separation, unavailable/stale/unsupported metadata and allowlisted clipboard commands without runtime access");
}
