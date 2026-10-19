#include "terminal_search.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void check(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void write(GhosttyTerminal terminal,const QByteArray &text){ghostty_terminal_vt_write(terminal,reinterpret_cast<const uint8_t *>(text.constData()),size_t(text.size()));}
static void settled(QtTerminalSearch &search){QElapsedTimer timer;timer.start();while(search.state()["busy"].toBool() && timer.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(1);}check(!search.state()["busy"].toBool(),"bounded scan completes after pause");}
static quint64 offset(GhosttyTerminal terminal){GhosttyTerminalScrollbar bar{};ghostty_terminal_get(terminal,GHOSTTY_TERMINAL_DATA_SCROLLBAR,&bar);return bar.offset;}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);GhosttyTerminal terminal=nullptr;
    check(ghostty_terminal_new(nullptr,&terminal,30,6)==GHOSTTY_SUCCESS,"terminal creation");
    {
        QtTerminalSearch search(terminal,nullptr);
        for(int i=0;i<100;++i)write(terminal,"line needle\r\n");
        const auto live=offset(terminal);check(search.begin(),"begin");search.setQuery("needle");settled(search);
        check(search.state()["count"]==100 && search.state()["selected"]==100 && !search.spans().isEmpty(),"history matches and initial last selection");
        search.move(1);check(search.state()["selected"]==1 && offset(terminal)<live,"wrap to oldest history");
        search.move(-1);check(search.state()["selected"]==100,"reverse wrap");search.close();check(offset(terminal)==live && search.spans().isEmpty(),"close restores live view");
        GhosttyTerminalScrollViewport scroll{};scroll.tag=GHOSTTY_SCROLL_VIEWPORT_ROW;scroll.value.row=10;ghostty_terminal_scroll_viewport(terminal,scroll);const auto retained=offset(terminal);
        search.begin();search.setQuery("needle");settled(search);search.close();check(offset(terminal)==retained,"tracked retained viewport restoration");
        search.begin();search.setQuery(QString(256,'x'));check(!search.state()["error"].toString().isEmpty() && !search.state()["busy"].toBool(),"query bound");
        search.setQuery(QString::fromUtf8("界").repeated(86));check(!search.state()["error"].toString().isEmpty(),"UTF-8 byte bound");
        search.setQuery("two\nlines");check(!search.state()["error"].toString().isEmpty(),"single-line query");
        search.setQuery("needle");search.invalidate();check(search.spans().isEmpty() && search.state()["count"]==0,"mutation immediately invalidates matches");write(terminal,"new needle\r\n");settled(search);check(search.state()["count"]==101,"rescan after output");
        search.invalidate();write(terminal,"\x1b[?1049hALT-only\r\n");settled(search);check(search.state()["count"]==0,"alternate screen isolation");
        search.setQuery("ALT-only");settled(search);check(search.state()["count"]==1,"active alternate screen search");search.close();
        write(terminal,"\x1b[?1049l\x1b[3J\x1b[2J\x1b[H界e\xcc\x81💻");search.begin();search.setQuery(QString::fromUtf8("界e\xcc\x81💻"));settled(search);
        check(search.state()["count"]==1 && !search.spans().isEmpty() && search.spans().first().cells.width()==5,"wide/combining Unicode cell highlights");
        search.close();write(terminal,"\x1b[3J\x1b[2J\x1b[H12345678901234567890123456789abcdef");search.begin();search.setQuery("89abc");settled(search);
        check(search.state()["count"]==1 && search.spans().size()==2,"soft-wrapped highlight spans");
        search.invalidate();check(ghostty_terminal_resize(terminal,16,8,0,0)==GHOSTTY_SUCCESS,"reflow");settled(search);check(search.state()["count"]==1,"rescan after reflow");
        search.close();write(terminal,"\x1b[3J\x1b[2J\x1b[Hab\r\ncd");search.begin();search.setQuery("bc");settled(search);check(search.state()["count"]==0,"hard newlines remain boundaries");
        search.close();write(terminal,"\x1b[3J\x1b[2J\x1b[H");for(int i=0;i<1100;++i)write(terminal,"x\r\n");search.begin();search.setQuery("x");settled(search);check(search.state()["count"]==1024 && search.state()["limited"].toBool(),"bounded matches");
        search.close();search.begin();search.setQuery("x");search.close(false);QCoreApplication::processEvents();check(!search.active() && search.spans().isEmpty(),"cancel pending timer before replacement");
    }
    ghostty_terminal_free(terminal);std::puts("PASS: Qt search history/live and tracked viewport restoration, wrap, UTF-8 bounds and highlights, invalidation, alternate screens, result cap and cancellation");
}
