#pragma once
#include <QObject>
#include <QJsonObject>
#include <QString>

// Client launch facts, captured once. This adapter never queries the service.
class UiBuildInfo final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary CONSTANT)
    Q_PROPERTY(QString diagnostics READ diagnostics CONSTANT)
public:
    explicit UiBuildInfo(const QJsonObject &configuration, QObject *parent=nullptr);
    QString summary() const { return summary_; }
    QString diagnostics() const { return diagnostics_; }
    Q_INVOKABLE void copyDiagnostics() const;
private:
    QString summary_;
    QString diagnostics_;
};
