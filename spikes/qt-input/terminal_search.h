#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <QRect>
#include <ghostty/vt.h>
struct WorkbenchSearch;
// Lazy client-only search. VT mutations and scan batches run serially on the GUI thread.
class QtTerminalSearch:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
    struct Span {QRect cells;bool selected=false;};
    QtTerminalSearch(GhosttyTerminal,QObject *parent);
    ~QtTerminalSearch() override;
    QVariantMap state() const;
    void setTerminal(GhosttyTerminal terminal){close(false);terminal_=terminal;}
    bool active()const{return active_;}
    const QVector<Span> &spans()const{return spans_;}
    void invalidate();
    void viewportChanged();
    Q_INVOKABLE bool begin();
    Q_INVOKABLE void setQuery(const QString &);
    Q_INVOKABLE void move(int);
    Q_INVOKABLE void close(bool restore=true);
Q_SIGNALS:
    void changed();
    void viewportRequested();
private:
    void tick();
    GhosttyTerminal terminal_;WorkbenchSearch *scan_=nullptr;
    GhosttyTrackedGridRef bookmark_=nullptr;GhosttyTerminalScreen screen_=GHOSTTY_TERMINAL_SCREEN_PRIMARY;
    bool active_=false,bottom_=false,busy_=false,started_=false;
    QString query_,error_;int count_=0,selected_=-1;bool limited_=false;
    QTimer timer_;QVector<Span> spans_;QByteArray mask_;
};
