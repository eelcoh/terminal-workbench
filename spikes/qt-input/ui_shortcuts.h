#pragma once
#include <QObject>
#include <QVariantMap>
class UiShortcuts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap sequences READ sequences CONSTANT)
    Q_PROPERTY(QVariantMap labels READ labels CONSTANT)
    Q_PROPERTY(QString error READ error CONSTANT)
public:
    explicit UiShortcuts(const QString &path,bool apple,QObject *parent=nullptr);
    QVariantMap sequences() const{return sequences_;}
    QVariantMap labels() const{return labels_;}
    QString error() const{return error_;}
private:
    QVariantMap sequences_,labels_;QString error_;
};
