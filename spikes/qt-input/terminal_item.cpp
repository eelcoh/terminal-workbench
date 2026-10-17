#include "terminal_item.h"
#include "platform_keys.h"
#include "terminal_accessibility.h"
#include <QPainter>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QClipboard>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QQuickWindow>
#include <QtEndian>
#include <algorithm>
#include <cmath>

namespace {
constexpr qsizetype maxSnapshot = 64 * 1024 * 1024;
constexpr qsizetype maxQueue = 1024 * 1024;
template<typename T> T sized() { T value{};value.size=sizeof(T);return value; }
QColor color(GhosttyColorRgb c) { return QColor(c.r,c.g,c.b); }
GhosttyKey key(int k) {
    if (k>=Qt::Key_A && k<=Qt::Key_Z) return GhosttyKey(GHOSTTY_KEY_A+k-Qt::Key_A);
    if (k>=Qt::Key_0 && k<=Qt::Key_9) return GhosttyKey(GHOSTTY_KEY_DIGIT_0+k-Qt::Key_0);
    if (k>=Qt::Key_F1 && k<=Qt::Key_F12) return GhosttyKey(GHOSTTY_KEY_F1+k-Qt::Key_F1);
    switch(k) {
    case Qt::Key_Return: case Qt::Key_Enter: return GHOSTTY_KEY_ENTER;
    case Qt::Key_Backspace: return GHOSTTY_KEY_BACKSPACE;
    case Qt::Key_Tab: case Qt::Key_Backtab: return GHOSTTY_KEY_TAB;
    case Qt::Key_Escape: return GHOSTTY_KEY_ESCAPE;
    case Qt::Key_Left: return GHOSTTY_KEY_ARROW_LEFT;
    case Qt::Key_Right: return GHOSTTY_KEY_ARROW_RIGHT;
    case Qt::Key_Up: return GHOSTTY_KEY_ARROW_UP;
    case Qt::Key_Down: return GHOSTTY_KEY_ARROW_DOWN;
    case Qt::Key_Home: return GHOSTTY_KEY_HOME;
    case Qt::Key_End: return GHOSTTY_KEY_END;
    case Qt::Key_PageUp: return GHOSTTY_KEY_PAGE_UP;
    case Qt::Key_PageDown: return GHOSTTY_KEY_PAGE_DOWN;
    case Qt::Key_Delete: return GHOSTTY_KEY_DELETE;
    case Qt::Key_Insert: return GHOSTTY_KEY_INSERT;
    case Qt::Key_Space: return GHOSTTY_KEY_SPACE;
    default: return GHOSTTY_KEY_UNIDENTIFIED;
    }
}
}

TerminalItem::TerminalItem(QQuickItem *parent): QQuickPaintedItem(parent)
{
    setFlag(ItemAcceptsInputMethod); setFlag(ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::LeftButton); setActiveFocusOnTab(true);
    setOpaquePainting(true);
    connect(this,&QQuickItem::windowChanged,this,[this](QQuickWindow *window) {
        if(window) {
            window->installEventFilter(this);
            connect(window,&QWindow::visibilityChanged,this,[this,window](QWindow::Visibility) {
                if(this->window()==window && ready_ && !refreshTimer_.isActive())refreshTimer_.start();
            });
        }
    });
    connect(this,&QQuickItem::visibleChanged,this,[this] {
        if(isVisible() && ready_ && !refreshTimer_.isActive())refreshTimer_.start();
    });
    const int fontId=QFontDatabase::addApplicationFont(":/fonts/fira-code/FiraCode-Regular.ttf");
    const auto families=QFontDatabase::applicationFontFamilies(fontId);
    if(families.isEmpty()) {fail("Bundled Fira Code could not load");return;}
    font_=QFont(families.first()); font_.setPixelSize(18);
    QFontMetricsF metrics(font_);
    cellWidth_=std::ceil(metrics.horizontalAdvance('M'));
    cellHeight_=std::ceil(metrics.height()); ascent_=metrics.ascent();
    if (ghostty_render_state_new(nullptr,&render_)!=GHOSTTY_SUCCESS
        || ghostty_render_state_row_iterator_new(nullptr,&rows_)!=GHOSTTY_SUCCESS
        || ghostty_render_state_row_cells_new(nullptr,&cells_)!=GHOSTTY_SUCCESS
        || ghostty_key_encoder_new(nullptr,&encoder_)!=GHOSTTY_SUCCESS
        || ghostty_key_event_new(nullptr,&event_)!=GHOSTTY_SUCCESS) {
        fail("Ghostty initialization failed"); return;
    }
    refreshTimer_.setSingleShot(true); refreshTimer_.setInterval(16);
    connect(&refreshTimer_,&QTimer::timeout,this,&TerminalItem::refresh);
    handshakeTimer_.setSingleShot(true); handshakeTimer_.setInterval(10000);
    connect(&handshakeTimer_,&QTimer::timeout,this,[this]{fail("Attachment timed out");});
    connect(this,&TerminalItem::statusChanged,this,[this]{
        if(!QAccessible::isActive())return;
        QAccessibleEvent description(this,QAccessible::DescriptionChanged);
        QAccessible::updateAccessibility(&description);
        QAccessible::State changed;changed.busy=true;
        QAccessibleStateChangeEvent state(this,changed);QAccessible::updateAccessibility(&state);
    });
    socket_.setReadBufferSize(256*1024);
    connect(&socket_,&QLocalSocket::readyRead,this,&TerminalItem::receive);
    connect(&socket_,&QLocalSocket::connected,this,[this]{
        socket_.write("ATS1\nf64f4aca2c29b554d111b36c3d946a9bddd159ff");
    });
    connect(&socket_,&QLocalSocket::disconnected,this,[this]{if(!failed_)fail("Connection closed");});
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](auto){if(!failed_)fail(socket_.errorString());});
}
TerminalItem::~TerminalItem()
{
    socket_.disconnect(this); socket_.abort();
    ghostty_key_event_free(event_); ghostty_key_encoder_free(encoder_);
    ghostty_render_state_row_cells_free(cells_); ghostty_render_state_row_iterator_free(rows_);
    ghostty_render_state_free(render_); ghostty_terminal_free(terminal_);
}
void TerminalItem::connectSession(const QString &path)
{
    if (failed_ || socket_.state()!=QLocalSocket::UnconnectedState) return;
    handshakeTimer_.start(); socket_.connectToServer(path);
}
void TerminalItem::fail(const QString &message)
{
    failed_=true; ready_=false; status_=message; snapshot_.clear(); receive_.clear();
    handshakeTimer_.stop(); socket_.abort(); Q_EMIT statusChanged();
}
void TerminalItem::send(quint8 kind,const QByteArray &data)
{
    if (!ready_ || failed_) return;
    if (data.size()>65536 || socket_.bytesToWrite()+data.size()+5>maxQueue) { fail("Input queue limit reached");return; }
    QByteArray packet(5,0);packet[0]=char(kind);
    qToBigEndian<quint32>(quint32(data.size()),packet.data()+1);packet+=data;
    if(socket_.write(packet)!=packet.size()) fail("Input write failed");
}
void TerminalItem::receive()
{
    queued_=false;
    // Bound work per event-loop turn, including large snapshot transfers.
    for(int count=0;count<64 && !failed_;++count) {
        if(receive_.size()<5) receive_+=socket_.read(5-receive_.size());
        if(receive_.size()<5) return;
        auto size=qFromBigEndian<quint32>(receive_.constData()+1);
        if(size>65536) {fail("Invalid frame length");return;}
        receive_+=socket_.read(5+size-receive_.size());
        if(receive_.size()<5+size) return;
        auto payload=receive_.mid(5);auto kind=quint8(receive_[0]);receive_.clear();
        if(!frame(kind,payload)) {if(!failed_)fail("Invalid attachment stream");return;}
    }
    if(!failed_ && socket_.bytesAvailable() && !queued_) {
        queued_=true;QTimer::singleShot(0,this,&TerminalItem::receive);
    }
}
bool TerminalItem::frame(quint8 kind,const QByteArray &data)
{
    if(kind==4) {fail(QString::fromUtf8(data));return false;}
    if(kind==7 && !snapshotStarted_ && !ready_ && data.size()==4) {
        snapshotSize_=qFromBigEndian<quint32>(data.constData());
        if(!snapshotSize_ || snapshotSize_>maxSnapshot)return false;
        snapshotStarted_=true;return true;
    }
    if(kind==8 && snapshotStarted_ && !restored_ && snapshot_.size()+data.size()<=snapshotSize_) {
        snapshot_+=data;return true;
    }
    if(kind==9 && snapshotStarted_ && !restored_ && data.isEmpty() && snapshot_.size()==snapshotSize_) {
        GhosttySnapshotDecoder decoder=nullptr;
        if(ghostty_snapshot_decoder_new_buf(nullptr,&decoder,
            reinterpret_cast<const uint8_t *>(snapshot_.constData()),snapshot_.size())!=GHOSTTY_SUCCESS)return false;
        auto result=ghostty_snapshot_decoder_decode(decoder,&terminal_);
        ghostty_snapshot_decoder_free(decoder);snapshot_.clear();
        if(result!=GHOSTTY_SUCCESS)return false;
        restored_=true;
        ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_USERDATA,this);
        ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_WRITE_PTY,reinterpret_cast<const void *>(reply));
        return true;
    }
    if(kind==2 && restored_ && data.size()==4) {
        columns_=qFromBigEndian<quint16>(data.constData());lines_=qFromBigEndian<quint16>(data.constData()+2);
        if(!columns_ || !lines_ || columns_>1000 || lines_>1000)return false;
        if(ghostty_terminal_resize(terminal_,columns_,lines_,0,0)!=GHOSTTY_SUCCESS)return false;
    } else if(kind==3 && restored_ && !ready_ && data.isEmpty()) {
        ready_=true;status_="Attached · experimental Qt painter";handshakeTimer_.stop();
        Q_EMIT statusChanged(); resizeTerminal();
    } else if(kind==1 && ready_) {
        ghostty_terminal_vt_write(terminal_,reinterpret_cast<const uint8_t *>(data.constData()),data.size());
        receivedBytes_+=quint64(data.size());
    } else return false;
    if(!refreshTimer_.isActive())refreshTimer_.start();
    return true;
}
void TerminalItem::reply(GhosttyTerminal,void *userdata,const uint8_t *bytes,size_t size)
{
    auto *self=static_cast<TerminalItem *>(userdata);
    if(size<=65536)self->send(16,QByteArray(reinterpret_cast<const char *>(bytes),qsizetype(size)));
    else self->fail("Terminal reply limit reached");
}
void TerminalItem::geometryChange(const QRectF &next,const QRectF &old)
{
    QQuickPaintedItem::geometryChange(next,old);
    update(boundingRect().toAlignedRect());resizeTerminal();
}
void TerminalItem::resizeTerminal()
{
    if(!ready_)return;
    quint16 cols=quint16(std::clamp(int((width()-16)/cellWidth_),1,1000));
    quint16 rows=quint16(std::clamp(int((height()-16)/cellHeight_),1,1000));
    if(cols==wantedColumns_ && rows==wantedLines_)return;
    wantedColumns_=cols;wantedLines_=rows;dragging_=false;
    QByteArray data(4,0);qToBigEndian(cols,data.data());qToBigEndian(rows,data.data()+2);send(17,data);
}
void TerminalItem::refresh()
{
    if(!terminal_ || failed_)return;
    const auto oldCaret=caret_;const bool oldCursorVisible=cursorVisible_;const auto oldBackground=background_;
    const bool notify=QAccessible::isActive() && isVisible();
    const auto previous=notify ? accessibleSnapshot() : AccessibleSnapshot();
    if(ghostty_render_state_update(render_,terminal_)!=GHOSTTY_SUCCESS) {fail("Render snapshot failed");return;}
    GhosttyRenderStateColors colors=sized<GhosttyRenderStateColors>();
    ghostty_render_state_colors_get(render_,&colors);background_=color(colors.background);
    grid_.clear();
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,&rows_);
    while(ghostty_render_state_row_iterator_next(rows_)) {
        QVector<Cell> row;
        ghostty_render_state_row_get(rows_,GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,&cells_);
        while(ghostty_render_state_row_cells_next(cells_)) {
            Cell cell;GhosttyColorRgb fg=colors.foreground,bg=colors.background;
            ghostty_render_state_row_cells_get(cells_,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR,&fg);
            ghostty_render_state_row_cells_get(cells_,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR,&bg);
            GhosttyStyle style=sized<GhosttyStyle>();
            ghostty_render_state_row_cells_get(cells_,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE,&style);
            if(style.inverse)std::swap(fg,bg);
            bool selected=false;
            ghostty_render_state_row_cells_get(cells_,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_SELECTED,&selected);
            cell.selected=selected;
            GhosttyCell raw=0;
            ghostty_render_state_row_cells_get(cells_,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW,&raw);
            ghostty_cell_get(raw,GHOSTTY_CELL_DATA_WIDE,&cell.wide);
            cell.fg=selected ? QColor("#ffffff") : color(fg);cell.bg=selected ? QColor("#405575") : color(bg);
            cell.bold=style.bold;cell.italic=style.italic;cell.underline=style.underline!=0;
            GhosttyBuffer buffer{};
            auto result=ghostty_render_state_row_cells_get(cells_,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,&buffer);
            if(result==GHOSTTY_OUT_OF_SPACE && buffer.len<=65536) {
                QByteArray bytes(qsizetype(buffer.len),0);buffer.ptr=reinterpret_cast<uint8_t *>(bytes.data());buffer.cap=bytes.size();
                if(ghostty_render_state_row_cells_get(cells_,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,&buffer)==GHOSTTY_SUCCESS)
                    cell.text=QString::fromUtf8(bytes.constData(),qsizetype(buffer.len));
            }
            row.push_back(std::move(cell));
        }
        grid_.push_back(std::move(row));
    }
    // Shape/rasterize only changed rows on the GUI thread. Paint receives images;
    // QRawFont glyph runs must not be moved to Qt Quick's render thread.
    const bool presentable=isVisible() && window() && window()->visibility()!=QWindow::Hidden && window()->visibility()!=QWindow::Minimized;
    const auto damage=textRows_.update(grid_,font_,cellWidth_,cellHeight_,ascent_,window()?window()->devicePixelRatio():1,presentable);
    bool visible=false,inViewport=false;quint16 x=0,y=0;
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE,&visible);
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE,&inViewport);
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X,&x);
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y,&y);
    caret_=QRectF(8+x*cellWidth_,8+y*cellHeight_,cellWidth_,cellHeight_);cursorVisible_=visible&&inViewport;
    refreshedBytes_=receivedBytes_;
    accessibleDirty_=true;
    if(notify)terminalAccessibilityChanged(this,previous,accessibleSnapshot());
    QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
    if(!presentable)return;
    // Use explicit bounds for a full request, so a later partial request in
    // the same event-loop turn cannot narrow Qt's pending dirty rectangle.
    if(damage.full || oldBackground!=background_ || composition_.active()) {update(boundingRect().toAlignedRect());return;}
    QRect dirty=damage.rect(boundingRect(),cellHeight_);
    if(oldCaret!=caret_ || oldCursorVisible!=cursorVisible_) {
        if(oldCursorVisible)dirty=dirty.united(oldCaret.adjusted(-1,-1,1,1).toAlignedRect());
        if(cursorVisible_)dirty=dirty.united(caret_.adjusted(-1,-1,1,1).toAlignedRect());
    }
    dirty=dirty.intersected(boundingRect().toAlignedRect());
    if(!dirty.isEmpty())update(dirty);
}
QString TerminalItem::viewportText() const
{
    QString text;
    for(const auto &row:grid_) {for(const auto &cell:row)text+=cell.text.isEmpty()?QStringLiteral(" "):cell.text;text+='\n';}
    return text;
}
void TerminalItem::paint(QPainter *painter)
{
    // Pure drawing of GUI-thread snapshots: no socket/VT calls or state mutation.
    painter->fillRect(boundingRect(),background_);
    painter->setRenderHint(QPainter::TextAntialiasing,true);
    paintTerminalGrid(*painter,grid_,textRows_,cellWidth_,cellHeight_,QPointF(8,8));
    if(cursorVisible_) {painter->setPen(QColor("#a8c7fa"));painter->drawRect(caret_.adjusted(0,0,-1,-1));}
    composition_.paint(painter,caret_,ascent_,background_);
}
QVariant TerminalItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if(query==Qt::ImEnabled)return ready_;
    if(query==Qt::ImCursorRectangle)return composition_.caret(caret_);
    if(query==Qt::ImFont)return font_;
    if(query==Qt::ImHints)return int(Qt::ImhNoPredictiveText|Qt::ImhNoAutoUppercase);
    // A terminal cannot safely replace shell-owned surrounding input.
    if(query==Qt::ImSurroundingText || query==Qt::ImCurrentSelection)return QString();
    if(query==Qt::ImCursorPosition || query==Qt::ImAnchorPosition)return 0;
    return QQuickPaintedItem::inputMethodQuery(query);
}
void TerminalItem::live()
{
    GhosttyTerminalScrollViewport scroll{};scroll.tag=GHOSTTY_SCROLL_VIEWPORT_BOTTOM;
    ghostty_terminal_scroll_viewport(terminal_,scroll);
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_SELECTION,nullptr);
    if(!refreshTimer_.isActive())refreshTimer_.start();
}
void TerminalItem::committed(const QString &text)
{
    if(!ready_ || text.isEmpty())return;
    live();auto bytes=text.toUtf8();
    // Composed text is committed once, independent of physical key reports.
    for(qsizetype offset=0;offset<bytes.size();offset+=65536)send(16,bytes.mid(offset,65536));
}
void TerminalItem::inputMethodEvent(QInputMethodEvent *event)
{
    if(!ready_) {event->ignore();return;}
    if(event->replacementStart()!=0 || event->replacementLength()!=0) {
        // Do not invent destructive edits to text owned by the shell.
        event->ignore();return;
    }
    composition_.set(*event,font_);committed(event->commitString());
    QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);update(boundingRect().toAlignedRect());event->accept();
}
void TerminalItem::encodeKey(QKeyEvent *input,bool release)
{
    if(!ready_)return;
    if(input->key()==Qt::Key_Shift || input->key()==Qt::Key_Control || input->key()==Qt::Key_Alt || input->key()==Qt::Key_Meta)return;
    if(!release && composition_.active())return;
    if(const auto *motion=wordMotion(input->key(),input->modifiers())) {
        // Shell-word navigation is a shared terminal binding, not a generic
        // Alt-arrow encoding. Repeats emit once; release must emit nothing.
        if(!release) {live();send(16,QByteArray(motion));}
        input->accept();return;
    }
    auto mods=terminalMods(input->modifiers());
    auto text=release?QByteArray():input->text().toUtf8();
    // Qt supplies control bytes in text(); Ghostty expects the associated
    // printable character and applies Ctrl itself (including legacy encoding).
    if(!release && (mods&GHOSTTY_MODS_CTRL) && input->key()>=0x20 && input->key()<=0x7e) {
        int scalar=input->key();
        if(scalar>='A' && scalar<='Z' && !(mods&GHOSTTY_MODS_SHIFT))scalar+=32;
        text=QByteArray(1,char(scalar));
    }
    if(!release && text.isEmpty() && key(input->key())==GHOSTTY_KEY_UNIDENTIFIED)return;
    ghostty_key_encoder_setopt_from_terminal(encoder_,terminal_);
    ghostty_key_event_set_key(event_,key(input->key()));
    ghostty_key_event_set_action(event_,release?GHOSTTY_KEY_ACTION_RELEASE:input->isAutoRepeat()?GHOSTTY_KEY_ACTION_REPEAT:GHOSTTY_KEY_ACTION_PRESS);
    ghostty_key_event_set_mods(event_,mods);
    ghostty_key_event_set_consumed_mods(event_,!text.isEmpty() && (mods&GHOSTTY_MODS_SHIFT)?GHOSTTY_MODS_SHIFT:0);
    quint32 cp= input->key()>=Qt::Key_A && input->key()<=Qt::Key_Z ? quint32(input->key()+32) : 0;
    ghostty_key_event_set_unshifted_codepoint(event_,cp);
    ghostty_key_event_set_utf8(event_,text.isEmpty()?nullptr:text.constData(),text.size());
    char output[65536];size_t written=0;
    if(ghostty_key_encoder_encode(encoder_,event_,output,sizeof(output),&written)!=GHOSTTY_SUCCESS) {fail("Key encoding failed");return;}
    if(written) {live();send(16,QByteArray(output,qsizetype(written)));}
    input->accept();
}
bool TerminalItem::eventFilter(QObject *watched,QEvent *event)
{
    if(watched==window() && event->type()==QEvent::DevicePixelRatioChange) {
        update(boundingRect().toAlignedRect());
        if(!refreshTimer_.isActive())refreshTimer_.start();
    }
    return QQuickPaintedItem::eventFilter(watched,event);
}
bool TerminalItem::event(QEvent *event)
{
    // QQuickItem normally uses Tab for focus traversal; a focused terminal
    // must send it to the shell. Chrome remains traversable before entry.
    if(hasActiveFocus() && (event->type()==QEvent::KeyPress || event->type()==QEvent::KeyRelease)) {
        auto *keyEvent=static_cast<QKeyEvent *>(event);
        if(keyEvent->key()==Qt::Key_Tab || keyEvent->key()==Qt::Key_Backtab) {
            encodeKey(keyEvent,event->type()==QEvent::KeyRelease);return true;
        }
    }
    return QQuickPaintedItem::event(event);
}
void TerminalItem::keyPressEvent(QKeyEvent *event)
{
    if(copyKey(event)) {copy();event->accept();return;}
    if(pasteKey(event)) {paste();event->accept();return;}
    encodeKey(event,false);
}
void TerminalItem::keyReleaseEvent(QKeyEvent *event)
{
    if(copyKey(event) || pasteKey(event)) {event->accept();return;}
    if(event->isAutoRepeat())return;
    encodeKey(event,true);
}
void TerminalItem::copy()
{
    if(!ready_)return;
    GhosttyTerminalSelectionFormatOptions options=sized<GhosttyTerminalSelectionFormatOptions>();
    options.emit=GHOSTTY_FORMATTER_FORMAT_PLAIN;options.unwrap=true;options.trim=true;
    size_t length=0;auto result=ghostty_terminal_selection_format_buf(terminal_,options,nullptr,0,&length);
    if((result!=GHOSTTY_OUT_OF_SPACE && result!=GHOSTTY_SUCCESS) || length>65536)return;
    QByteArray bytes(qsizetype(length),0);
    if(ghostty_terminal_selection_format_buf(terminal_,options,reinterpret_cast<uint8_t *>(bytes.data()),length,&length)==GHOSTTY_SUCCESS)
        QGuiApplication::clipboard()->setText(QString::fromUtf8(bytes.constData(),qsizetype(length)));
}
void TerminalItem::paste()
{
    if(!ready_)return;
    auto text=QGuiApplication::clipboard()->text().toUtf8();if(text.isEmpty() || text.size()>65524)return;
    GhosttyTerminalModeConfig bracketed{};bracketed.mode=GHOSTTY_MODE_BRACKETED_PASTE;
    ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_MODE,&bracketed);
    QByteArray output(text.size()*2+12,0);size_t written=0;
    if(ghostty_paste_encode(text.data(),text.size(),bracketed.value,output.data(),output.size(),&written)==GHOSTTY_SUCCESS) {
        live();send(16,output.first(qsizetype(written)));
    }
}
void TerminalItem::select(const QPointF &position,bool start)
{
    if(!ready_ || !columns_ || !lines_)return;
    QPoint cell(std::clamp(int((position.x()-8)/cellWidth_),0,int(columns_)-1),std::clamp(int((position.y()-8)/cellHeight_),0,int(lines_)-1));
    if(start)anchor_=cell;
    GhosttySelection selection=sized<GhosttySelection>();
    GhosttyPoint point{};point.tag=GHOSTTY_POINT_TAG_VIEWPORT;
    point.value.coordinate={quint16(anchor_.x()),quint32(anchor_.y())};
    if(ghostty_terminal_grid_ref(terminal_,point,&selection.start)!=GHOSTTY_SUCCESS)return;
    point.value.coordinate={quint16(cell.x()),quint32(cell.y())};
    if(ghostty_terminal_grid_ref(terminal_,point,&selection.end)!=GHOSTTY_SUCCESS)return;
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_SELECTION,&selection);refresh();
}
void TerminalItem::mousePressEvent(QMouseEvent *event) {forceActiveFocus();dragging_=true;select(event->position(),true);event->accept();}
void TerminalItem::mouseMoveEvent(QMouseEvent *event) {if(dragging_)select(event->position(),false);event->accept();}
void TerminalItem::mouseReleaseEvent(QMouseEvent *event) {if(dragging_)select(event->position(),false);dragging_=false;event->accept();}
void TerminalItem::wheelEvent(QWheelEvent *event)
{
    if(!ready_)return;
    GhosttyTerminalScrollViewport scroll{};scroll.tag=GHOSTTY_SCROLL_VIEWPORT_DELTA;
    scroll.value.delta=event->pixelDelta().isNull() ? -event->angleDelta().y()/40 : -event->pixelDelta().y()/int(cellHeight_);
    dragging_=false;ghostty_terminal_scroll_viewport(terminal_,scroll);refresh();event->accept();
}
