#pragma once
#include <QQuickPaintedItem>
#include <QLocalSocket>
#include <QTimer>
#include <QFont>
#include "terminal_composition.h"
#include "terminal_text.h"
#include <ghostty/vt.h>

// A disposable rendering/input experiment. The runtime owns the PTY and VT;
// this client restores a snapshot and maintains only its presentation replica.
class TerminalItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit TerminalItem(QQuickItem *parent = nullptr);
    ~TerminalItem() override;
    void connectSession(const QString &control);
    QString status() const { return status_; }
    bool ready() const { return ready_; }
    QString preeditText() const { return composition_.text(); }
    QString viewportText() const;
    QRectF caret() const { return caret_; }
    qreal textRasterScale() const {return textRows_.size()==0?0:textRows_.image(0).devicePixelRatio();}
    // Narrow client diagnostics for the isolated live-runtime benchmark.
    // Queried at scenario boundaries; no aggregate collection in paint/parse.
    quint64 receivedBytes() const {return receivedBytes_;}
    quint64 refreshedBytes() const {return refreshedBytes_;}
    quint64 rasterizedRows() const {return textRows_.rasterizedRows();}
    qint64 rowImageBytes() const {return textRows_.imageBytes();}
    QSize terminalGridSize() const {return QSize(columns_,lines_);}
    QSizeF cellSize() const { return QSizeF(cellWidth_,cellHeight_); }
    struct AccessibleCharacter { QPoint cell; int width=1; bool selected=false; };
    struct AccessibleSnapshot {
        QString text;
        QVector<AccessibleCharacter> characters; // one entry per UTF-16 code unit
        int cursor=-1, selectionStart=-1, selectionEnd=-1;
    };
    const AccessibleSnapshot &accessibleSnapshot() const;
    void accessibleSelect(int start,int end);
    void accessibleClearSelection();
    void paint(QPainter *) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery) const override;
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();
Q_SIGNALS:
    void statusChanged();
protected:
    bool event(QEvent *) override;
    bool eventFilter(QObject *,QEvent *) override;
    void geometryChange(const QRectF &, const QRectF &) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void inputMethodEvent(QInputMethodEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
private:
    using Cell=TerminalTextCell;
    void fail(const QString &);
    void receive();
    bool frame(quint8, const QByteArray &);
    void send(quint8, const QByteArray &);
    void resizeTerminal();
    void refresh();
    void encodeKey(QKeyEvent *, bool);
    void committed(const QString &);
    void select(const QPointF &, bool);
    void live();
    static void reply(GhosttyTerminal, void *, const uint8_t *, size_t);
    QLocalSocket socket_;
    QTimer refreshTimer_, handshakeTimer_;
    QByteArray receive_, snapshot_;
    quint32 snapshotSize_ = 0;
    quint64 receivedBytes_=0,refreshedBytes_=0;
    bool snapshotStarted_=false, restored_=false, ready_=false, failed_=false, queued_=false;
    QString status_="Connecting";
    TerminalComposition composition_;
    GhosttyTerminal terminal_=nullptr;
    GhosttyRenderState render_=nullptr;
    GhosttyRenderStateRowIterator rows_=nullptr;
    GhosttyRenderStateRowCells cells_=nullptr;
    GhosttyKeyEncoder encoder_=nullptr;
    GhosttyKeyEvent event_=nullptr;
    QVector<QVector<Cell>> grid_;
    TerminalTextSurface textRows_;
    QColor background_{"#191d24"};
    QFont font_;
    qreal cellWidth_=10, cellHeight_=22, ascent_=17;
    quint16 columns_=0, lines_=0, wantedColumns_=0, wantedLines_=0;
    QRectF caret_;
    bool cursorVisible_=false, dragging_=false;
    QPoint anchor_;
    mutable AccessibleSnapshot accessible_;
    mutable bool accessibleDirty_=true;
};
