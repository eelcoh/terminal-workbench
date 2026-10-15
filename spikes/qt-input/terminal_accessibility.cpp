#include "terminal_accessibility.h"
#include <QAccessibleObject>
#include <QAccessibleTextInterface>
#include <QAccessibleActionInterface>
#include <QQuickWindow>
#include <QGuiApplication>
#include <algorithm>

const TerminalItem::AccessibleSnapshot &TerminalItem::accessibleSnapshot() const
{
    if(!accessibleDirty_)return accessible_;
    AccessibleSnapshot snapshot;
    int cursorRow=cursorVisible_ ? int((caret_.y()-8)/cellHeight_): -1;
    int cursorColumn=int((caret_.x()-8)/cellWidth_);
    int usedRows=std::max(1,cursorRow+1);
    for(int y=0;y<grid_.size();++y)
        for(const auto &cell:grid_[y])
            if((!cell.text.isEmpty() && cell.text!=QStringLiteral(" ")) || cell.selected) {usedRows=std::max(usedRows,y+1);break;}
    for(int y=0;y<std::min(usedRows,int(grid_.size()));++y) {
        const auto &row=grid_[y];int usedColumns=y==cursorRow?cursorColumn+1:0;
        for(int x=0;x<row.size();++x)
            if((!row[x].text.isEmpty() && row[x].text!=QStringLiteral(" ")) || row[x].selected)usedColumns=std::max(usedColumns,x+1);
        for(int x=0;x<std::min(usedColumns,int(row.size()));++x) {
            const auto &cell=row[x];
            if(cell.wide==GHOSTTY_CELL_WIDE_SPACER_TAIL || cell.wide==GHOSTTY_CELL_WIDE_SPACER_HEAD)continue;
            int width=cell.wide==GHOSTTY_CELL_WIDE_WIDE?2:1;
            int offset=int(snapshot.text.size());
            if(y==cursorRow && x<=cursorColumn && cursorColumn<x+width)snapshot.cursor=offset;
            const auto text=cell.text.isEmpty()?QStringLiteral(" "):cell.text;
            snapshot.text+=text;
            for(qsizetype unit=0;unit<text.size();++unit)snapshot.characters.push_back({QPoint(x,y),width,cell.selected});
            if(cell.selected) {
                if(snapshot.selectionStart<0)snapshot.selectionStart=offset;
                snapshot.selectionEnd=int(snapshot.text.size());
            }
        }
        snapshot.text+='\n';snapshot.characters.push_back({QPoint(std::max(0,usedColumns),y),1,false});
    }
    accessible_=std::move(snapshot);accessibleDirty_=false;return accessible_;
}
void TerminalItem::accessibleSelect(int start,int end)
{
    if(!ready_)return;
    const auto &snapshot=accessibleSnapshot();
    if(start<0 || end<=start || end>snapshot.characters.size())return;
    auto first=snapshot.characters[start].cell,last=snapshot.characters[end-1].cell;
    if(first.x()>=columns_ || last.x()>=columns_)return;
    GhosttySelection selection{};selection.size=sizeof(selection);
    GhosttyPoint point{};point.tag=GHOSTTY_POINT_TAG_VIEWPORT;
    point.value.coordinate={quint16(first.x()),quint32(first.y())};
    if(ghostty_terminal_grid_ref(terminal_,point,&selection.start)!=GHOSTTY_SUCCESS)return;
    point.value.coordinate={quint16(last.x()),quint32(last.y())};
    if(ghostty_terminal_grid_ref(terminal_,point,&selection.end)!=GHOSTTY_SUCCESS)return;
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_SELECTION,&selection);refresh();
}
void TerminalItem::accessibleClearSelection()
{
    if(!terminal_)return;
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_SELECTION,nullptr);refresh();
}
namespace {
class TerminalAccessible final : public QAccessibleObject, public QAccessibleTextInterface,
                                 public QAccessibleActionInterface {
public:
    explicit TerminalAccessible(TerminalItem *item): QAccessibleObject(item) {}
    TerminalItem *item() const { return qobject_cast<TerminalItem *>(object()); }
    QWindow *window() const override {return item()?item()->window():nullptr;}
    QAccessible::Role role() const override {return QAccessible::Terminal;}
    QAccessible::State state() const override {
        QAccessible::State result;
        if(!item()) {result.invalid=true;return result;}
        result.focusable=true;result.focused=item()->hasActiveFocus();
        result.readOnly=true;result.selectableText=true;result.multiLine=true;
        result.busy=!item()->ready();result.disabled=!item()->isEnabled();
        result.invisible=!item()->isVisible() || !window() || !window()->isVisible();
        return result;
    }
    QString text(QAccessible::Text type) const override {
        if(!item())return {};
        if(type==QAccessible::Name)return QStringLiteral("Terminal");
        if(type==QAccessible::Description)return item()->status();
        if(type==QAccessible::Value)return item()->accessibleSnapshot().text;
        return {};
    }
    QAccessibleInterface *parent() const override {
        if(!item())return nullptr;
        // The QML container is explicitly semantic, so use its public interface.
        if(auto *parent=item()->parentItem())return QAccessible::queryAccessibleInterface(parent);
        return QAccessible::queryAccessibleInterface(window());
    }
    int childCount() const override {return 0;}
    QAccessibleInterface *child(int) const override {return nullptr;}
    QAccessibleInterface *childAt(int,int) const override {return nullptr;}
    int indexOfChild(const QAccessibleInterface *) const override {return -1;}
    QAccessibleInterface *focusChild() const override {return item() && item()->hasActiveFocus()?const_cast<TerminalAccessible *>(this):nullptr;}
    QRect rect() const override {
        if(!item() || !window())return {};
        const auto scene=item()->mapRectToScene(item()->boundingRect());
        return QRect(item()->window()->mapToGlobal(scene.topLeft().toPoint()),scene.size().toSize());
    }
    void *interface_cast(QAccessible::InterfaceType type) override {
        if(type==QAccessible::TextInterface)return static_cast<QAccessibleTextInterface *>(this);
        if(type==QAccessible::ActionInterface)return static_cast<QAccessibleActionInterface *>(this);
        return nullptr; // No document editing interface: the shell owns its input.
    }
    QStringList actionNames() const override {return {QAccessibleActionInterface::setFocusAction()};}
    void doAction(const QString &action) override {
        if(item() && action==QAccessibleActionInterface::setFocusAction())item()->forceActiveFocus();
    }
    QStringList keyBindingsForAction(const QString &) const override {return {};}
    int characterCount() const override {return item()?int(item()->accessibleSnapshot().text.size()):0;}
    QString text(int start,int end) const override {
        if(!item())return {};
        const auto &text=item()->accessibleSnapshot().text;
        if(end==-1)end=int(text.size());
        if(start<0 || end<start || end>text.size())return {};
        return text.mid(start,end-start);
    }
    int cursorPosition() const override {return item()?item()->accessibleSnapshot().cursor:-1;}
    void setCursorPosition(int) override {} // Do not synthesize shell cursor/edit keys.
    int selectionCount() const override {return item() && item()->accessibleSnapshot().selectionStart>=0?1:0;}
    void selection(int index,int *start,int *end) const override {
        *start=*end=-1;
        if(item() && index==0) {*start=item()->accessibleSnapshot().selectionStart;*end=item()->accessibleSnapshot().selectionEnd;}
    }
    void addSelection(int start,int end) override {if(item())item()->accessibleSelect(start,end);}
    void setSelection(int index,int start,int end) override {if(index==0)addSelection(start,end);}
    void removeSelection(int index) override {if(item() && index==0)item()->accessibleClearSelection();}
    QRect characterRect(int offset) const override {
        if(!item() || !window())return {};
        const auto &positions=item()->accessibleSnapshot().characters;
        if(offset<0 || offset>=positions.size())return {};
        const auto &p=positions[offset];auto size=item()->cellSize();
        QPointF topLeft(8+p.cell.x()*size.width(),8+p.cell.y()*size.height());
        return QRect(item()->window()->mapToGlobal(item()->mapToScene(topLeft).toPoint()),QSize(int(size.width()*p.width),int(size.height())));
    }
    int offsetAtPoint(const QPoint &point) const override {
        if(!rect().contains(point) || !item())return -1;
        const auto local=item()->mapFromScene(item()->window()->mapFromGlobal(point));
        auto size=item()->cellSize();
        const int column=int((local.x()-8)/size.width()),row=int((local.y()-8)/size.height());
        if(local.x()<8 || local.y()<8)return -1;
        const auto &positions=item()->accessibleSnapshot().characters;
        for(int i=0;i<positions.size();++i) {
            const auto &p=positions[i];
            if(p.cell.y()==row && column>=p.cell.x() && column<p.cell.x()+p.width)return i;
        }
        return -1;
    }
    void scrollToSubstring(int,int) override {} // This interface describes only the visible viewport.
    QString attributes(int offset,int *start,int *end) const override {
        *start=*end=-1;
        if(offset<0 || offset>=characterCount())return {};
        *start=0;*end=characterCount();return QStringLiteral("read-only:true;");
    }
};
QAccessibleInterface *factory(const QString &,QObject *object) {
    if(auto *item=qobject_cast<TerminalItem *>(object))return new TerminalAccessible(item);
    return nullptr;
}
}
void installTerminalAccessibility() {QAccessible::installFactory(factory);}
void terminalAccessibilityChanged(TerminalItem *item,const TerminalItem::AccessibleSnapshot &before,
                                 const TerminalItem::AccessibleSnapshot &after)
{
    if(before.text!=after.text) {
        int prefix=0,oldEnd=int(before.text.size()),newEnd=int(after.text.size());
        while(prefix<oldEnd && prefix<newEnd && before.text[prefix]==after.text[prefix])++prefix;
        // Preserve UTF-16 scalar boundaries in platform text-change events.
        if(prefix>0 && prefix<newEnd && after.text[prefix].isLowSurrogate())--prefix;
        while(oldEnd>prefix && newEnd>prefix && before.text[oldEnd-1]==after.text[newEnd-1]) {--oldEnd;--newEnd;}
        if(newEnd<int(after.text.size()) && after.text[newEnd].isLowSurrogate()) {++oldEnd;++newEnd;}
        QAccessibleTextUpdateEvent event(item,prefix,before.text.mid(prefix,oldEnd-prefix),after.text.mid(prefix,newEnd-prefix));
        QAccessible::updateAccessibility(&event);
    }
    if(before.cursor!=after.cursor) {
        QAccessibleTextCursorEvent event(item,after.cursor);QAccessible::updateAccessibility(&event);
    }
    if(before.selectionStart!=after.selectionStart || before.selectionEnd!=after.selectionEnd) {
        QAccessibleTextSelectionEvent event(item,after.selectionStart,after.selectionEnd);QAccessible::updateAccessibility(&event);
    }
}
