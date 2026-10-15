#include "terminal_accessibility.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QDebug>
#include <cstdio>

bool terminalAccessibilitySelfTest(TerminalItem *item)
{
    QAccessible::setActive(true);
    auto *iface=QAccessible::queryAccessibleInterface(item);
    auto check=[](bool ok,const char *message) {if(!ok)qCritical()<<"FAIL: accessibility:"<<message;return ok;};
    if(!check(iface && iface->role()==QAccessible::Terminal,"terminal role"))return false;
    bool ok=check(iface->text(QAccessible::Name)=="Terminal","stable accessible name");
    auto *text=iface->textInterface();auto *action=iface->actionInterface();
    if(!check(text && action,"text and action interfaces"))return false;
    ok &= check(iface->editableTextInterface()==nullptr && iface->state().readOnly && !iface->state().editable,"no document editing interface");
    auto *parent=iface->parent();
    ok &= check(parent && parent->indexOfChild(iface)>=0 && parent->child(parent->indexOfChild(iface))==iface,"discoverable parent/child links");
    auto content=text->text(0,-1);
    ok &= check(content.contains(QString::fromUtf8("café λ")) && content.contains("QT-SNAPSHOT"),"visible Unicode text");
    ok &= check(!content.contains("history-") && content.size()<2000,"visible viewport only, trailing blanks trimmed");
    ok &= check(text->characterCount()==content.size() && text->text(-1,1).isEmpty() && text->text(2,1).isEmpty(),"text counts and invalid ranges");
    const auto combined=QString::fromUtf8("é"),wide=QString::fromUtf8("漢"),emoji=QString::fromUtf8("😀");
    int combining=content.indexOf(combined),wideIndex=content.indexOf(wide),emojiIndex=content.indexOf(emoji);
    ok &= check(combining>=0 && wideIndex>=0 && emojiIndex>=0,"cluster fixture visible");
    if(combining>=0)ok &= check(text->characterRect(combining)==text->characterRect(combining+1),"combining marks share cell geometry");
    if(wideIndex>=0)ok &= check(text->characterRect(wideIndex).width()==2*int(item->cellSize().width()),"wide character spans two cells");
    if(emojiIndex>=0) {
        auto rect=text->characterRect(emojiIndex);
        ok &= check(rect==text->characterRect(emojiIndex+1) && text->offsetAtPoint(rect.center())==emojiIndex,"surrogate pair and wide-tail point map to one cluster");
        text->addSelection(emojiIndex,emojiIndex+emoji.size());
        int start=-1,end=-1;text->selection(0,&start,&end);
        ok &= check(text->selectionCount()==1 && start==emojiIndex && end==emojiIndex+emoji.size(),"accessible selection round trip");
        item->copy();ok &= check(QGuiApplication::clipboard()->text()==emoji,"accessible selection uses actual VT copy");
        text->removeSelection(0);ok &= check(text->selectionCount()==0,"remove selection");
    }
    int cursor=text->cursorPosition();text->setCursorPosition(0);
    ok &= check(text->cursorPosition()==cursor,"document cursor request cannot move shell cursor");
    ok &= check(text->characterRect(-1).isEmpty() && text->characterRect(text->characterCount()).isEmpty(),"invalid character geometry");
    item->setFocus(false);
    ok &= check(!iface->state().focused,"focus state follows the actual item");
    action->doAction(QAccessibleActionInterface::setFocusAction());
    ok &= check(iface->state().focused && item->hasActiveFocus(),"accessible focus action");
    if(ok)std::puts("PASS: terminal accessibility role/tree, visible Unicode text, cluster geometry, VT selection/copy and focus; no document editing");
    return ok;
}
