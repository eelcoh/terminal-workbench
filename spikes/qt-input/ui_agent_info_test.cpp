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
        require(value["heading"]=="Status unknown" && value["launcher"].toString().startsWith("wb-"),"recognition does not establish lifecycle status");
        require(help.copyLauncher(agent) && QGuiApplication::clipboard()->text()==value["launcher"].toString(),"only an allowlisted launcher is copied");
    }
    const auto previous=QGuiApplication::clipboard()->text();
    require(!help.copyLauncher("pi; echo injected") && QGuiApplication::clipboard()->text()==previous,"arbitrary metadata cannot become clipboard commands");
    value=help.describe({{"agent_available",true},{"agent","Gemini"},{"state","Working"}});
    require(value["launcher"].toString().isEmpty() && value["heading"]=="Working","unsupported launcher does not discard valid status");
    value=help.describe({{"agent_available",true},{"agent","Codex"},{"state","invented"}});
    require(value["heading"]=="Status unknown","unknown state retains honest explanation");
    std::puts("PASS: identity/status separation, unavailable/stale/unsupported metadata and allowlisted clipboard commands without runtime access");
}
