#include "ui_agent_info.h"
#include <QGuiApplication>
#include <QClipboard>
namespace {
QString launcher(const QString &agent){
    if(agent=="Codex")return "wb-codex";
    if(agent=="Claude")return "wb-claude";
    if(agent=="Copilot")return "wb-copilot";
    if(agent=="Pi")return "wb-pi";
    return {};
}
}
QVariantMap UiAgentInfo::describe(const QVariantMap &facts) const {
    const auto name=facts.value("agent").toString();const bool available=facts.value("agent_available").toBool();
    const auto state=facts.value("state").toString();
    QString heading,meaning;
    if(!available){heading="Agent information unavailable";meaning="This runtime has not supplied agent metadata. Existing terminal input still works.";}
    else if(name.isEmpty()){heading="No agent detected";meaning="The foreground process is not a recognized agent. Identification uses process metadata, independently of terminal text.";}
    else if(state=="Working"){heading=state;meaning="The last lifecycle report says the agent is processing a turn.";}
    else if(state=="Needs input"){heading=state;meaning="The last lifecycle report says the agent is waiting for an answer or approval. Return to its terminal to respond.";}
    else if(state=="Ready"){heading=state;meaning="The last lifecycle report says the turn finished. This does not establish that its task succeeded.";}
    else {heading="Status unknown";meaning="The process is recognized, but no usable lifecycle state is available. Plain launches, disabled observers and older runtimes can produce this state.";}
    return {{"agent",available?name:QString()},{"heading",heading},{"meaning",meaning},{"launcher",available?launcher(name):QString()}};
}
bool UiAgentInfo::copyLauncher(const QString &agent) const {
    const auto command=launcher(agent);if(command.isEmpty())return false;
    QGuiApplication::clipboard()->setText(command);return true;
}
