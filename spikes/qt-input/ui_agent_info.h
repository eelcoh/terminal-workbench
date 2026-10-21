#pragma once
#include <QObject>
#include <QVariantMap>
// Presentation of already-delivered metadata; no runtime access or process launch.
class UiAgentInfo final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE QVariantMap describe(const QVariantMap &facts) const;
    Q_INVOKABLE bool copyLauncher(const QString &agent) const;
};
