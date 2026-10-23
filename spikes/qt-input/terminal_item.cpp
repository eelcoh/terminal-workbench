#include <QFontInfo>
#include "terminal_item.h"
#include "terminal_search.h"
#include "local_socket.h"
#include "theme_bridge.h"
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
#include <QStyleHints>
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
    connect(this,&QQuickItem::activeFocusChanged,this,&TerminalItem::updateCursor);
    connect(this,&QQuickItem::enabledChanged,this,&TerminalItem::updateCursor);
    connect(this,&QQuickItem::windowChanged,this,[this](QQuickWindow *window) {
        if(window) {
            window->installEventFilter(this);
            connect(window,&QWindow::activeChanged,this,[this,window]{if(this->window()==window){if(!window->isActive())cancelForwarding();updateCursor();}});
            connect(window,&QWindow::visibilityChanged,this,[this,window](QWindow::Visibility) {
                if(this->window()==window && ready_ && !refreshTimer_.isActive())refreshTimer_.start();
            });
        }
    });
    connect(this,&QQuickItem::visibleChanged,this,[this] {
        if(isVisible() && ready_ && !refreshTimer_.isActive())refreshTimer_.start();
    });
    connect(this,&QQuickItem::activeFocusChanged,this,[this]{if(!hasActiveFocus())cancelForwarding();});
    connect(this,&QQuickItem::enabledChanged,this,[this]{if(!isEnabled())cancelForwarding();});
    connect(this,&QQuickItem::visibleChanged,this,[this]{if(!isVisible())cancelForwarding();});
    forwardingTimer_.setSingleShot(true);forwardingTimer_.setInterval(10000);
    connect(&forwardingTimer_,&QTimer::timeout,this,&TerminalItem::cancelForwarding);
    // Register once per process, not once per pane.
    static const auto families=QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(":/fonts/fira-code/FiraCode-Regular.ttf"));
    if(families.isEmpty()) {fail("Bundled Fira Code could not load");return;}
    font_=QFont(families.first()); font_.setPixelSize(18);
    QFontMetricsF metrics(font_);
    cellWidth_=std::ceil(metrics.horizontalAdvance('M'));
    cellHeight_=std::ceil(metrics.height()); ascent_=metrics.ascent();
    if (ghostty_render_state_new(nullptr,&render_)!=GHOSTTY_SUCCESS
        || ghostty_render_state_row_iterator_new(nullptr,&rows_)!=GHOSTTY_SUCCESS
        || ghostty_render_state_row_cells_new(nullptr,&cells_)!=GHOSTTY_SUCCESS
        || ghostty_key_encoder_new(nullptr,&encoder_)!=GHOSTTY_SUCCESS
        || ghostty_key_event_new(nullptr,&event_)!=GHOSTTY_SUCCESS
        || ghostty_mouse_encoder_new(nullptr,&mouse_)!=GHOSTTY_SUCCESS
        || ghostty_mouse_event_new(nullptr,&mouseEvent_)!=GHOSTTY_SUCCESS
        || ghostty_selection_gesture_new(nullptr,&gesture_)!=GHOSTTY_SUCCESS
        || ghostty_selection_gesture_event_new(nullptr,&press_,GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS)!=GHOSTTY_SUCCESS
        || ghostty_selection_gesture_event_new(nullptr,&drag_,GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG)!=GHOSTTY_SUCCESS
        || ghostty_selection_gesture_event_new(nullptr,&release_,GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE)!=GHOSTTY_SUCCESS) {
        fail("Ghostty initialization failed"); return;
    }
    setAcceptHoverEvents(true);
    refreshTimer_.setSingleShot(true); refreshTimer_.setInterval(16);
    connect(&refreshTimer_,&QTimer::timeout,this,[this]{refresh();});
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
        if(legacy_)return;
        socket_.write((takeover_ ? QByteArray("ATM1\n") : QByteArray("ATS1\n"))+"f64f4aca2c29b554d111b36c3d946a9bddd159ff");
    });
    connect(&socket_,&QLocalSocket::disconnected,this,[this]{if(!failed_ && !attachmentFallback(false))fail("Connection closed");});
    connect(&socket_,&QLocalSocket::errorOccurred,this,[this](auto error){
        if(failed_)return;
        const bool unavailable=error==QLocalSocket::ServerNotFoundError || error==QLocalSocket::ConnectionRefusedError;
        if(unavailable && attachmentFallback(true))return;
        // Peer close is handled after buffered frames have been drained.
        if(error!=QLocalSocket::PeerClosedError)fail(socket_.errorString());
    });
}
TerminalItem::~TerminalItem()
{
    delete search_;search_=nullptr;
    socket_.disconnect(this); socket_.abort();
    ghostty_key_event_free(event_); ghostty_key_encoder_free(encoder_);
    ghostty_mouse_event_free(mouseEvent_); ghostty_mouse_encoder_free(mouse_);
    ghostty_selection_gesture_event_free(press_); ghostty_selection_gesture_event_free(drag_);
    ghostty_selection_gesture_event_free(release_); ghostty_selection_gesture_free(gesture_,terminal_);
    ghostty_render_state_row_cells_free(cells_); ghostty_render_state_row_iterator_free(rows_);
    ghostty_render_state_free(render_); ghostty_terminal_free(terminal_);
}
void TerminalItem::connectSession(const QString &path)
{
    if (failed_ || socket_.state()!=QLocalSocket::UnconnectedState) return;
    controlPath_=path;legacy_=false;attachmentSeen_=false;replayBytes_=0;
    ++connectionAttempts_;handshakeTimer_.start(); socket_.connectToServer(path);
}
void TerminalItem::reconnectSession(const QString &path,bool takeover) {
    if(!render_ || !encoder_ || !event_ || attachment_=="connecting" || ready_)return;
    failed_=true;socket_.abort();handshakeTimer_.stop();refreshTimer_.stop();
    if(search_)search_->close(false);
    receive_.clear();snapshot_.clear();snapshotSize_=0;snapshotStarted_=restored_=queued_=false;
    wheel_.reset();wheelTime_.invalidate();
    wantedColumns_=wantedLines_=0;takeover_=takeover;controlPath_=path;legacy_=false;attachmentSeen_=false;replayBytes_=0;
    QGuiApplication::inputMethod()->reset();composition_.set(QInputMethodEvent(),font_);
    failed_=false;attachment_="connecting";status_="Connecting";Q_EMIT statusChanged();
    ++connectionAttempts_;handshakeTimer_.start();socket_.connectToServer(path);
}
bool TerminalItem::attachmentFallback(bool unavailable) {
    // Match the working viewer's bounded compatibility path. Never downgrade
    // after any response, rejection, malformed snapshot, or successful attach.
    if(failed_ || ready_ || legacy_ || attachmentSeen_ || !receive_.isEmpty() || !controlPath_.endsWith(".ctl"))return false;
    const bool replay=unavailable || !takeover_;
    const auto path=replay ? controlPath_.chopped(4)+".sock" : controlPath_;
    if(!ownedLocalSocket(path))return false;
    failed_=true;socket_.abort();handshakeTimer_.stop();takeover_=false;
    legacy_=replay;restored_=false;replayBytes_=0;
    if(legacy_){
        if(!terminal_){
            if(ghostty_terminal_new(nullptr,&terminal_,80,24)!=GHOSTTY_SUCCESS){fail("Could not initialize legacy terminal.");return true;}
        } else ghostty_terminal_reset(terminal_);
        applyTheme();
        ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_USERDATA,this);
        ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_WRITE_PTY,reinterpret_cast<const void *>(reply));
    }
    QTimer::singleShot(0,this,[this,path]{
        failed_=false;status_=legacy_ ? "Connecting · legacy replay" : "Connecting";
        ++connectionAttempts_;handshakeTimer_.start();Q_EMIT statusChanged();socket_.connectToServer(path);
    });
    return true;
}
void TerminalItem::setThemeIndex(int index)
{
    WorkbenchTheme theme;
    if(index==themeIndex_ || !workbench_theme_get(index,&theme))return;
    themeIndex_=index;applyTheme();Q_EMIT themeChanged();
    if(terminal_ && !refreshTimer_.isActive())refreshTimer_.start();
}
void TerminalItem::applyTheme()
{
    if(!terminal_ || themeIndex_<0)return;
    WorkbenchTheme theme;if(!workbench_theme_get(themeIndex_,&theme))return;
    const auto rgb=[](WorkbenchThemeColor c){return GhosttyColorRgb{c.r,c.g,c.b};};
    auto fg=rgb(theme.foreground),bg=rgb(theme.background),cursor=rgb(theme.cursor);
    GhosttyColorRgb palette[256];for(int i=0;i<256;++i)palette[i]=rgb(theme.palette[i]);
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND,&fg);
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND,&bg);
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_COLOR_CURSOR,&cursor);
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_COLOR_PALETTE,&palette);
    selectionBackground_=QColor(theme.selection_background.r,theme.selection_background.g,theme.selection_background.b);
    selectionForeground_=QColor(theme.selection_foreground.r,theme.selection_foreground.g,theme.selection_foreground.b);
}
void TerminalItem::setTextFont(const QVariantMap &style)
{
    const auto family=style.value("family").toString();const auto size=style.value("size");
    if((family!="Fira Code" && family!="JetBrains Mono") || size.toDouble()!=size.toInt() || size.toInt()<10 || size.toInt()>32)return;
    if(font_.family()==family && fontPoints_==size.toInt())return;
    // Only explicitly bundled faces; never silently select an installed fallback.
    static QHash<QString,QString> loaded;
    if(!loaded.contains(family)) {
        const auto resource=family=="Fira Code" ? ":/fonts/fira-code/FiraCode-Regular.ttf" : ":/fonts/jetbrains-mono/JetBrainsMono-Regular.ttf";
        const auto names=QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(resource));
        if(names.isEmpty()){qWarning("Bundled terminal font unavailable");return;}loaded.insert(family,names.first());
    }
    // Clear old composition before changing its metrics; never commit preedit.
    if(composition_.active() && hasActiveFocus())QGuiApplication::inputMethod()->reset();
    composition_.set(QInputMethodEvent(),font_);
    wheel_.reset();wheelTime_.invalidate();
    font_=QFont(loaded.value(family));fontPoints_=size.toInt();font_.setPointSizeF(fontPoints_);
    QFontMetricsF metrics(font_);cellWidth_=std::ceil(metrics.horizontalAdvance('M'));cellHeight_=std::ceil(metrics.height());ascent_=metrics.ascent();
    resetGesture();cursorGlyph_=QImage();cursorRowKey_=0;
    resizeTerminal();if(ready_ && !refreshTimer_.isActive())refreshTimer_.start();
    update(boundingRect().toAlignedRect());QGuiApplication::inputMethod()->update(Qt::ImFont|Qt::ImCursorRectangle);Q_EMIT textFontChanged();
}
void TerminalItem::setPresentationEnabled(bool enabled)
{
    if(presentationEnabled_==enabled)return;
    presentationEnabled_=enabled;
    if(!enabled)cancelForwarding();
    Q_EMIT presentationEnabledChanged();
    // Hidden sources keep parsing; one refresh catches the latest grid on reveal.
    if(enabled && !refreshTimer_.isActive())refreshTimer_.start();
}
void TerminalItem::fail(const QString &message,const QString &attachment)
{
    cancelForwarding();
    failed_=true; ready_=false; status_=message;attachment_=attachment; snapshot_.clear(); receive_.clear();
    handshakeTimer_.stop(); socket_.abort(); updateCursor();Q_EMIT statusChanged();
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
    attachmentSeen_=true;
    if(kind==4) {const auto message=QString::fromUtf8(data);fail(message,message=="a window is already attached" ? "elsewhere" : "disconnected");return false;}
    if(kind==10 && data.isEmpty()) {fail("Open in another window","elsewhere");return false;}
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
        GhosttyTerminal next=nullptr;
        auto result=ghostty_snapshot_decoder_decode(decoder,&next);
        ghostty_snapshot_decoder_free(decoder);snapshot_.clear();
        if(result!=GHOSTTY_SUCCESS) {ghostty_terminal_free(next);return false;}
        ghostty_render_state_row_cells_free(cells_);cells_=nullptr;
        ghostty_render_state_row_iterator_free(rows_);rows_=nullptr;
        ghostty_render_state_free(render_);render_=nullptr;
        if(search_)search_->close(false);
        resetGesture();ghostty_terminal_free(terminal_);terminal_=next;
        if(search_)search_->setTerminal(terminal_);
        if(ghostty_render_state_new(nullptr,&render_)!=GHOSTTY_SUCCESS
            || ghostty_render_state_row_iterator_new(nullptr,&rows_)!=GHOSTTY_SUCCESS
            || ghostty_render_state_row_cells_new(nullptr,&cells_)!=GHOSTTY_SUCCESS)return false;
        cursorGlyph_=QImage();cursorRowKey_=0;cursorVisible_=false;accessibleDirty_=true;
        restored_=true;applyTheme();
        ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_USERDATA,this);
        ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_WRITE_PTY,reinterpret_cast<const void *>(reply));
        return true;
    }
    if(kind==2 && (restored_ || legacy_) && data.size()==4) {
        columns_=qFromBigEndian<quint16>(data.constData());lines_=qFromBigEndian<quint16>(data.constData()+2);
        if(!columns_ || !lines_ || columns_>1000 || lines_>1000)return false;
        if(search_)search_->invalidate();
        if(ghostty_terminal_resize(terminal_,columns_,lines_,0,0)!=GHOSTTY_SUCCESS)return false;
    } else if(kind==3 && (restored_ || legacy_) && !ready_ && data.isEmpty()) {
        ready_=true;attachment_="attached";status_=legacy_ ? "Attached · legacy replay" : "Attached · experimental Qt painter";handshakeTimer_.stop();
        Q_EMIT statusChanged(); resizeTerminal();
    } else if(kind==1 && (ready_ || legacy_)) {
        if(!ready_){replayBytes_+=quint64(data.size());if(replayBytes_>16*1024*1024){fail("Legacy replay exceeds its 16 MiB limit.");return false;}}
        if(search_)search_->invalidate();
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
    wantedColumns_=cols;wantedLines_=rows;resetGesture();
    QByteArray data(4,0);qToBigEndian(cols,data.data());qToBigEndian(rows,data.data()+2);send(17,data);
}
void TerminalItem::refresh(bool retained)
{
    if(!terminal_ || !render_ || (failed_ && !retained))return;
    const auto oldCursorStyle=cursorStyle_;const auto oldCursorColor=cursorColor_;const auto oldCaret=caret_;const bool oldCursorVisible=cursorVisible_;const auto oldBackground=background_;
    const bool notify=QAccessible::isActive() && isVisible();
    const auto previous=notify ? accessibleSnapshot() : AccessibleSnapshot();
    if(ghostty_render_state_update(render_,terminal_)!=GHOSTTY_SUCCESS) {fail("Render snapshot failed");return;}
    GhosttyRenderStateColors colors=sized<GhosttyRenderStateColors>();
    ghostty_render_state_colors_get(render_,&colors);background_=color(colors.background);
    GhosttyColorRgb cursor{};
    cursorColor_=ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_COLOR_CURSOR,&cursor)==GHOSTTY_SUCCESS?color(cursor):QColor("#a8c7fa");
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
            cell.fg=selected ? selectionForeground_ : color(fg);cell.bg=selected ? selectionBackground_ : color(bg);
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
    if(search_)search_->viewportChanged();
    // Shape/rasterize only changed rows on the GUI thread. Paint receives images;
    // QRawFont glyph runs must not be moved to Qt Quick's render thread.
    const bool presentable=presentationEnabled_ && isVisible() && window() && window()->visibility()!=QWindow::Hidden && window()->visibility()!=QWindow::Minimized;
    const auto damage=textRows_.update(grid_,font_,cellWidth_,cellHeight_,ascent_,window()?window()->devicePixelRatio():1,presentable);
    bool visible=false,inViewport=false;quint16 x=0,y=0;
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE,&visible);
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE,&inViewport);
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X,&x);
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y,&y);
    ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VISUAL_STYLE,&cursorStyle_);
    bool wideTail=false;ghostty_render_state_get(render_,GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_WIDE_TAIL,&wideTail);
    // Keep the cursor over the whole grapheme when VT reports its wide tail.
    if(wideTail && x>0)--x;
    const bool wide=y<grid_.size() && x<grid_[y].size() && grid_[y][x].wide==GHOSTTY_CELL_WIDE_WIDE;
    caret_=QRectF(8+x*cellWidth_,8+y*cellHeight_,cellWidth_*(wide?2:1),cellHeight_);cursorVisible_=visible&&inViewport;
    updateCursor();
    refreshedBytes_=receivedBytes_;
    accessibleDirty_=true;
    if(notify)terminalAccessibilityChanged(this,previous,accessibleSnapshot());
    QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
    if(!presentable)return;
    // Use explicit bounds for a full request, so a later partial request in
    // the same event-loop turn cannot narrow Qt's pending dirty rectangle.
    if(damage.full || oldBackground!=background_ || composition_.active()) {update(boundingRect().toAlignedRect());return;}
    QRect dirty=damage.rect(boundingRect(),cellHeight_);
    if(oldCaret!=caret_ || oldCursorVisible!=cursorVisible_ || oldCursorColor!=cursorColor_ || oldCursorStyle!=cursorStyle_) {
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
QString TerminalItem::cursorPresentation() const
{
    if(!cursorVisible_)return "hidden";
    if(!cursorFocused_ || cursorStyle_==GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK_HOLLOW)return "outline";
    if(cursorStyle_==GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BAR)return "bar";
    if(cursorStyle_==GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_UNDERLINE)return "underline";
    return "block";
}
void TerminalItem::updateCursor()
{
    const bool focused=ready_ && hasActiveFocus() && isEnabled() && window() && window()->isActive();
    const bool changed=focused!=cursorFocused_;cursorFocused_=focused;
    bool glyphChanged=false;
    if(presentationEnabled_ && isVisible() && cursorVisible_ && focused && cursorStyle_==GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK) {
        const int row=qRound((caret_.y()-8)/cellHeight_),column=qRound((caret_.x()-8)/cellWidth_);
        if(row>=0 && row<textRows_.size() && row<grid_.size() && column>=0 && column<grid_[row].size()) {
            const auto &source=textRows_.image(row);const qreal scale=source.devicePixelRatio();
            const QRect rect(qRound(column*cellWidth_*scale),0,qCeil(caret_.width()*scale),qCeil(cellHeight_*scale));
            const auto ink=grid_[row][column].bg;
            if(cursorRowKey_!=source.cacheKey() || cursorSource_!=rect || cursorInk_!=ink) {
                cursorRowKey_=source.cacheKey();cursorSource_=rect;cursorInk_=ink;cursorGlyph_=QImage();
                // Reuse the shaped foreground mask; never shape or allocate in paint.
                if(!source.isNull() && source.rect().intersects(rect)) {
                    cursorGlyph_=source.copy(rect);cursorGlyph_.setDevicePixelRatio(1);
                    {QPainter painter(&cursorGlyph_);painter.setCompositionMode(QPainter::CompositionMode_SourceIn);painter.fillRect(cursorGlyph_.rect(),ink);}
                    cursorGlyph_.setDevicePixelRatio(scale);
                }
                glyphChanged=true;
            }
        }
    }
    if((changed || glyphChanged) && presentationEnabled_ && isVisible() && cursorVisible_)
        update(caret_.adjusted(-1,-1,1,1).toAlignedRect());
}
void TerminalItem::paint(QPainter *painter)
{
    // Pure drawing of GUI-thread snapshots: no socket/VT calls or state mutation.
    painter->fillRect(boundingRect(),background_);
    painter->setRenderHint(QPainter::TextAntialiasing,true);
    paintTerminalGrid(*painter,grid_,textRows_,cellWidth_,cellHeight_,QPointF(8,8));
    if(search_ && search_->active())for(const auto &span:search_->spans()) {
        const auto &cells=span.cells;
        const QRectF rect(8+cells.x()*cellWidth_,8+cells.y()*cellHeight_,cells.width()*cellWidth_,cellHeight_);
        QColor ink=cursorColor_;ink.setAlpha(span.selected?85:38);painter->fillRect(rect,ink);
        if(span.selected){const qreal stroke=1/(window()?window()->devicePixelRatio():1);QPen pen(cursorColor_);pen.setWidthF(stroke);painter->setPen(pen);painter->setBrush(Qt::NoBrush);painter->drawRect(rect.adjusted(stroke/2,stroke/2,-stroke/2,-stroke/2));}
    }
    if(cursorVisible_) {
        if(!cursorFocused_ || cursorStyle_==GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK_HOLLOW) {
            const qreal stroke=1/(window()?window()->devicePixelRatio():1);
            QPen pen(cursorColor_);pen.setWidthF(stroke);painter->setPen(pen);painter->setBrush(Qt::NoBrush);
            painter->drawRect(caret_.adjusted(stroke/2,stroke/2,-stroke/2,-stroke/2));
        } else if(cursorStyle_==GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BAR) {
            painter->fillRect(QRectF(caret_.topLeft(),QSizeF(std::min(qreal(2),caret_.width()),caret_.height())),cursorColor_);
        } else if(cursorStyle_==GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_UNDERLINE) {
            painter->fillRect(QRectF(caret_.left(),caret_.bottom()-2,caret_.width(),2),cursorColor_);
        } else {
            painter->fillRect(caret_,cursorColor_);
            if(!cursorGlyph_.isNull())painter->drawImage(caret_.topLeft(),cursorGlyph_);
        }
    }
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
    wheel_.reset();wheelTime_.invalidate();
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
    cancelForwarding();
    composition_.set(*event,font_);committed(event->commitString());
    QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);update(boundingRect().toAlignedRect());event->accept();
}
void TerminalItem::encodeKey(QKeyEvent *input,bool release,bool raw)
{
    if(!ready_)return;
    if(input->key()==Qt::Key_Shift || input->key()==Qt::Key_Control || input->key()==Qt::Key_Alt || input->key()==Qt::Key_Meta)return;
    if(!release && composition_.active())return;
    if(const auto *motion=raw?nullptr:wordMotion(input->key(),input->modifiers())) {
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
bool TerminalItem::forwardNextKey()
{
    if(!ready_ || !hasActiveFocus() || !isEnabled() || !presentationEnabled_ || !window() || !window()->isActive() || composition_.active())return false;
    forwardedKey_=0;cancelledKey_=0;forwardingArmed_=true;forwardingTimer_.start();Q_EMIT forwardingChanged();return true;
}
void TerminalItem::cancelForwarding()
{
    forwardingTimer_.stop();forwardedKey_=0;cancelledKey_=0;
    if(forwardingArmed_){forwardingArmed_=false;Q_EMIT forwardingChanged();}
}
bool TerminalItem::event(QEvent *event)
{
    if(hasActiveFocus() && event->type()==QEvent::ShortcutOverride) {
        const auto *input=static_cast<QKeyEvent *>(event);
        if(forwardingArmed_ || (forwardedKey_ && input->key()==forwardedKey_)){event->accept();return true;}
    }
    if(hasActiveFocus() && (event->type()==QEvent::KeyPress || event->type()==QEvent::KeyRelease)) {
        auto *input=static_cast<QKeyEvent *>(event);const bool release=event->type()==QEvent::KeyRelease;
        const int key=input->key();
        if(key==cancelledKey_){if(release && !input->isAutoRepeat())cancelledKey_=0;event->accept();return true;}
        if(forwardingArmed_ && !release && key!=Qt::Key_Control && key!=Qt::Key_Meta && key!=Qt::Key_Shift && key!=Qt::Key_Alt && key!=Qt::Key_AltGr) {
            if(input->isAutoRepeat()){event->accept();return true;}
            forwardingArmed_=false;forwardingTimer_.stop();
            if(key==Qt::Key_Escape && input->modifiers()==Qt::NoModifier)cancelledKey_=key;
            else forwardedKey_=key;
            Q_EMIT forwardingChanged();
            if(cancelledKey_){event->accept();return true;}
        }
        if(forwardedKey_ && key==forwardedKey_) {
            if(!(release && input->isAutoRepeat()))encodeKey(input,release,true);
            if(release && !input->isAutoRepeat())forwardedKey_=0;
            event->accept();return true;
        }
    }
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
bool TerminalItem::mouseTracking() const
{
    bool tracking=false;
    if(terminal_)ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING,&tracking);
    return tracking;
}
static GhosttyMouseButton mouseButton(Qt::MouseButton button)
{
    switch(button) {
    case Qt::LeftButton: return GHOSTTY_MOUSE_BUTTON_LEFT;
    case Qt::RightButton: return GHOSTTY_MOUSE_BUTTON_RIGHT;
    case Qt::MiddleButton: return GHOSTTY_MOUSE_BUTTON_MIDDLE;
    case Qt::BackButton: return GHOSTTY_MOUSE_BUTTON_EIGHT;
    case Qt::ForwardButton: return GHOSTTY_MOUSE_BUTTON_NINE;
    default: return GHOSTTY_MOUSE_BUTTON_UNKNOWN;
    }
}
// The encoder follows the application's tracking mode and format, and emits
// nothing for events that mode does not report.
bool TerminalItem::reportMouse(GhosttyMouseAction action,GhosttyMouseButton button,const QPointF &position,Qt::KeyboardModifiers modifiers,bool anyPressed)
{
    if(!ready_ || !columns_ || !lines_)return false;
    ghostty_mouse_encoder_setopt_from_terminal(mouse_,terminal_);
    auto size=sized<GhosttyMouseEncoderSize>();
    size.screen_width=quint32(width());size.screen_height=quint32(height());
    size.cell_width=quint32(std::max<qreal>(1,cellWidth_));size.cell_height=quint32(std::max<qreal>(1,cellHeight_));
    size.padding_left=size.padding_top=8;
    size.padding_right=quint32(std::max<qreal>(0,width()-8-columns_*cellWidth_));
    size.padding_bottom=quint32(std::max<qreal>(0,height()-8-lines_*cellHeight_));
    ghostty_mouse_encoder_setopt(mouse_,GHOSTTY_MOUSE_ENCODER_OPT_SIZE,&size);
    ghostty_mouse_encoder_setopt(mouse_,GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED,&anyPressed);
    const bool trackCell=true;ghostty_mouse_encoder_setopt(mouse_,GHOSTTY_MOUSE_ENCODER_OPT_TRACK_LAST_CELL,&trackCell);
    ghostty_mouse_event_set_action(mouseEvent_,action);
    if(button==GHOSTTY_MOUSE_BUTTON_UNKNOWN)ghostty_mouse_event_clear_button(mouseEvent_);
    else ghostty_mouse_event_set_button(mouseEvent_,button);
    ghostty_mouse_event_set_mods(mouseEvent_,terminalMods(modifiers));
    ghostty_mouse_event_set_position(mouseEvent_,GhosttyMousePosition{float(position.x()),float(position.y())});
    char bytes[128];size_t written=0;
    if(ghostty_mouse_encoder_encode(mouse_,mouseEvent_,bytes,sizeof(bytes),&written)!=GHOSTTY_SUCCESS || !written)return false;
    send(16,QByteArray(bytes,qsizetype(written)));return true;
}
void TerminalItem::resetGesture()
{
    if(gesture_ && terminal_)ghostty_selection_gesture_reset(gesture_,terminal_);
    dragging_=false;
}
// VT owns anchors, click counting and word/line boundaries.
bool TerminalItem::gesture(GhosttySelectionGestureEvent event,const QPointF &position,qint64 timestamp)
{
    if(!ready_ || !columns_ || !lines_)return false;
    QPoint cell(std::clamp(int((position.x()-8)/cellWidth_),0,int(columns_)-1),std::clamp(int((position.y()-8)/cellHeight_),0,int(lines_)-1));
    GhosttyPoint point{};point.tag=GHOSTTY_POINT_TAG_VIEWPORT;
    point.value.coordinate={quint16(cell.x()),quint32(cell.y())};
    auto ref=sized<GhosttyGridRef>();
    if(ghostty_terminal_grid_ref(terminal_,point,&ref)!=GHOSTTY_SUCCESS)return false;
    if(event==press_) {
        const auto *hints=QGuiApplication::styleHints();
        const quint64 time=quint64(std::max<qint64>(0,timestamp))*1000000ULL;
        const quint64 interval=quint64(hints->mouseDoubleClickInterval())*1000000ULL;
        const double distance=hints->mouseDoubleClickDistance();
        ghostty_selection_gesture_event_set(event,GHOSTTY_SELECTION_GESTURE_EVENT_OPT_TIME_NS,&time);
        ghostty_selection_gesture_event_set(event,GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_INTERVAL_NS,&interval);
        ghostty_selection_gesture_event_set(event,GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_DISTANCE,&distance);
    } else if(event==drag_) {
        GhosttySelectionGestureGeometry geometry{columns_,quint32(std::max<qreal>(1,cellWidth_)),8,quint32(std::max<qreal>(1,height()))};
        ghostty_selection_gesture_event_set(event,GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY,&geometry);
    }
    if(event!=release_) {
        GhosttySurfacePosition surface{position.x(),position.y()};
        ghostty_selection_gesture_event_set(event,GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION,&surface);
    }
    if(ghostty_selection_gesture_event_set(event,GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF,&ref)!=GHOSTTY_SUCCESS)return false;
    auto selection=sized<GhosttySelection>();
    if(ghostty_selection_gesture_event(gesture_,terminal_,event,&selection)!=GHOSTTY_SUCCESS)return false;
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_SELECTION,&selection);refresh();return true;
}
void TerminalItem::mousePressEvent(QMouseEvent *event)
{
    cancelForwarding();forceActiveFocus();event->accept();
    // Shift selects text even while an application tracks the mouse.
    if(reporting_ || (mouseTracking() && !(event->modifiers()&Qt::ShiftModifier))) {
        reporting_=true;
        reportMouse(GHOSTTY_MOUSE_ACTION_PRESS,mouseButton(event->button()),event->position(),event->modifiers(),true);
        return;
    }
    if(event->button()!=Qt::LeftButton)return;
    accessibleClearSelection();
    ghostty_terminal_set(terminal_,GHOSTTY_TERMINAL_OPT_SELECTION,nullptr);
    dragging_=true;gesture(press_,event->position(),event->timestamp());
}
void TerminalItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    // The press preceding this event already advanced the VT click count.
    event->accept();
}
void TerminalItem::mouseMoveEvent(QMouseEvent *event)
{
    event->accept();
    if(reporting_) {
        GhosttyMouseButton held=GHOSTTY_MOUSE_BUTTON_UNKNOWN;
        for(auto button:{Qt::LeftButton,Qt::RightButton,Qt::MiddleButton})
            if(event->buttons()&button){held=mouseButton(button);break;}
        reportMouse(GHOSTTY_MOUSE_ACTION_MOTION,held,event->position(),event->modifiers(),event->buttons()!=Qt::NoButton);
        return;
    }
    if(dragging_ && (event->buttons()&Qt::LeftButton))gesture(drag_,event->position());
}
void TerminalItem::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
    if(reporting_) {
        reportMouse(GHOSTTY_MOUSE_ACTION_RELEASE,mouseButton(event->button()),event->position(),event->modifiers(),event->buttons()!=Qt::NoButton);
        if(event->buttons()==Qt::NoButton)reporting_=false;
        return;
    }
    if(dragging_ && event->button()==Qt::LeftButton){gesture(release_,event->position());dragging_=false;}
}
void TerminalItem::hoverMoveEvent(QHoverEvent *event)
{
    // Any-event tracking reports motion without buttons; other modes emit nothing.
    if(!dragging_ && mouseTracking())reportMouse(GHOSTTY_MOUSE_ACTION_MOTION,GHOSTTY_MOUSE_BUTTON_UNKNOWN,event->position(),event->modifiers(),false);
    event->ignore();
}
QObject *TerminalItem::searchController()
{
    if(!search_) {
        search_=new QtTerminalSearch(terminal_,this);
        connect(search_,&QtTerminalSearch::viewportRequested,this,[this]{refresh(true);update(boundingRect().toAlignedRect());});
        connect(search_,&QtTerminalSearch::changed,this,[this]{update(boundingRect().toAlignedRect());});
    }
    return search_;
}
// Explicit display diagnostics only; never called by parsing or painting.
QVariantMap TerminalItem::renderDiagnostics() const
{
    const QFontInfo actual(font_);
    const qreal dpr=window()?window()->devicePixelRatio():1;
    const auto origin=mapToScene(QPointF(8,8));
    return {{"requested_family",font_.family()},{"requested_points",font_.pointSizeF()},
        {"resolved_family",actual.family()},{"resolved_style",actual.styleName()},
        {"resolved_points",actual.pointSizeF()},{"resolved_pixels",actual.pixelSize()},
        {"resolved_weight",actual.weight()},{"exact_font_match",actual.exactMatch()},
        {"cell_width",cellWidth_},{"cell_height",cellHeight_},{"ascent",ascent_},
        {"row_image_dpr",textRasterScale()},{"columns",columns_},{"rows",lines_},
        {"origin_device_x",origin.x()*dpr},{"origin_device_y",origin.y()*dpr},
        {"presented",presentationEnabled_ && isVisible()},{"ready",ready_},
        {"background",background_.name()},{"paint_target",int(renderTarget())}};
}
void TerminalItem::wheelEvent(QWheelEvent *event)
{
    if(!ready_){event->ignore();return;}
    if(!wheelTime_.isValid() || wheelTime_.elapsed()>300)wheel_.reset();
    wheelTime_.start();
    // X11 pixel deltas are driver-specific; Qt recommends angle deltas there.
    const auto result=wheel_.consume(event->pixelDelta(),event->angleDelta(),cellHeight_,
        QGuiApplication::styleHints()->wheelScrollLines(),QGuiApplication::platformName()!=QStringLiteral("xcb"),event->phase());
    if(qEnvironmentVariableIsSet("WORKBENCH_SCROLL_DEBUG"))
        qInfo("Qt scroll platform=%s pixel=(%d,%d) angle=(%d,%d) inverted=%d phase=%d rows=%d handled=%d",
            qPrintable(QGuiApplication::platformName()),event->pixelDelta().x(),event->pixelDelta().y(),event->angleDelta().x(),event->angleDelta().y(),event->inverted(),int(event->phase()),result.rows,result.handled);
    if(!result.handled){event->ignore();return;}
    event->accept();
    if(!result.rows)return;
    if(mouseTracking() && !(event->modifiers()&Qt::ShiftModifier)) {
        // Applications that track the mouse receive wheel buttons 4/5 instead.
        const auto button=result.rows<0?GHOSTTY_MOUSE_BUTTON_FOUR:GHOSTTY_MOUSE_BUTTON_FIVE;
        for(int step=0;step<std::min(std::abs(result.rows),12);++step) {
            reportMouse(GHOSTTY_MOUSE_ACTION_PRESS,button,event->position(),event->modifiers(),false);
            reportMouse(GHOSTTY_MOUSE_ACTION_RELEASE,button,event->position(),event->modifiers(),false);
        }
        return;
    }
    GhosttyTerminalScrollViewport scroll{};scroll.tag=GHOSTTY_SCROLL_VIEWPORT_DELTA;scroll.value.delta=result.rows;
    resetGesture();ghostty_terminal_scroll_viewport(terminal_,scroll);refresh();
}
