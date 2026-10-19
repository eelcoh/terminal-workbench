#include "terminal_search.h"
#include "search_bridge.h"
#include <QElapsedTimer>
QtTerminalSearch::QtTerminalSearch(GhosttyTerminal terminal,QObject *parent):QObject(parent),terminal_(terminal){
    timer_.setSingleShot(true);connect(&timer_,&QTimer::timeout,this,&QtTerminalSearch::tick);
}
QtTerminalSearch::~QtTerminalSearch(){close(false);workbench_search_free(scan_);}
QVariantMap QtTerminalSearch::state()const {
    return {{"active",active_},{"busy",busy_},{"count",count_},{"selected",selected_+1},{"limited",limited_},{"error",error_},{"highlight_spans",spans_.size()}};
}
bool QtTerminalSearch::begin(){
    close();if(!scan_)scan_=workbench_search_new();if(!scan_)return false;
    ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_VIEWPORT_ACTIVE,&bottom_);
    ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN,&screen_);
    GhosttyPoint point{};point.tag=GHOSTTY_POINT_TAG_VIEWPORT;
    ghostty_terminal_grid_ref_track(terminal_,point,&bookmark_);
    active_=true;query_.clear();error_.clear();count_=0;selected_=-1;limited_=false;busy_=false;started_=false;
    Q_EMIT changed();return true;
}
void QtTerminalSearch::close(bool restore){
    if(!active_ && !bookmark_)return;
    active_=false;busy_=false;timer_.stop();spans_.clear();spans_.squeeze();mask_.clear();mask_.squeeze();
    GhosttyTerminalScreen screen=GHOSTTY_TERMINAL_SCREEN_PRIMARY;ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN,&screen);
    if(restore && screen==screen_) {
        GhosttyTerminalScrollViewport scroll{};bool valid=false;
        if(bottom_){scroll.tag=GHOSTTY_SCROLL_VIEWPORT_BOTTOM;valid=true;}
        else if(bookmark_){GhosttyPointCoordinate point{};if(ghostty_tracked_grid_ref_point(bookmark_,GHOSTTY_POINT_TAG_SCREEN,&point)==GHOSTTY_SUCCESS){scroll.tag=GHOSTTY_SCROLL_VIEWPORT_ROW;scroll.value.row=point.y;valid=true;}}
        if(valid)ghostty_terminal_scroll_viewport(terminal_,scroll);
    }
    ghostty_tracked_grid_ref_free(bookmark_);bookmark_=nullptr;query_.clear();error_.clear();
    Q_EMIT changed();if(restore)Q_EMIT viewportRequested();
}
void QtTerminalSearch::setQuery(const QString &query){
    if(!active_)return;
    const auto bytes=query.toUtf8();
    query_=query;error_=bytes.size()>255 || query.contains(QChar(10)) || query.contains(QChar(13)) || query.contains(QChar(0))?QStringLiteral("Use one line of at most 255 UTF-8 bytes"):QString();
    invalidate();
}
void QtTerminalSearch::invalidate(){
    if(!active_)return;
    const bool notify=!busy_ || !spans_.isEmpty() || count_!=0;
    started_=false;spans_.clear();count_=0;selected_=-1;limited_=false;
    busy_=!query_.isEmpty() && error_.isEmpty();
    timer_.stop();if(busy_)timer_.start(120);
    if(notify || !busy_)Q_EMIT changed();
}
void QtTerminalSearch::tick(){
    if(!active_ || !busy_)return;
    if(!started_) {
        const auto bytes=query_.toUtf8();
        if(!workbench_search_begin(scan_,terminal_,bytes.constData())){busy_=false;error_="Could not read retained terminal text";Q_EMIT changed();return;}
        started_=true;
    }
    QElapsedTimer elapsed;elapsed.start();bool scanning=true;
    for(int cells=0;cells<4096 && elapsed.nsecsElapsed()<2000000;cells+=256){scanning=workbench_search_step(scan_,terminal_,256);if(!scanning)break;}
    if(scanning){timer_.start(1);return;}
    bool failed=false;workbench_search_status(scan_,&count_,&selected_,&limited_,&failed);busy_=false;
    if(failed)error_="Could not read retained terminal text";
    else if(count_) {workbench_search_move(scan_,terminal_,-1);workbench_search_status(scan_,&count_,&selected_,&limited_,&failed);Q_EMIT viewportRequested();}
    viewportChanged();Q_EMIT changed();
}
void QtTerminalSearch::move(int direction){
    if(!active_ || busy_ || !count_ || !error_.isEmpty())return;
    if(workbench_search_move(scan_,terminal_,direction<0?-1:1)){bool failed=false;workbench_search_status(scan_,&count_,&selected_,&limited_,&failed);Q_EMIT viewportRequested();viewportChanged();Q_EMIT changed();}
}
void QtTerminalSearch::viewportChanged(){
    if(!active_ || busy_ || !started_ || !error_.isEmpty())return;
    uint16_t columns=0,rows=0;
    ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_COLS,&columns);ghostty_terminal_get(terminal_,GHOSTTY_TERMINAL_DATA_ROWS,&rows);
    if(!columns || !rows || columns>1000 || rows>1000)return;
    mask_.resize(int(columns)*rows);int cols=0,height=0;spans_.clear();
    if(!workbench_search_mask(scan_,terminal_,reinterpret_cast<unsigned char *>(mask_.data()),size_t(mask_.size()),&cols,&height))return;
    for(int y=0;y<height;++y)for(int x=0;x<cols;){const auto ink=mask_[y*cols+x];const int start=x++;if(!ink)continue;while(x<cols && mask_[y*cols+x]==ink)++x;spans_.append({QRect(start,y,x-start,1),ink==2});}
}
