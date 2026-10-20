#pragma once
#include <QJsonObject>
#include <QStringList>
class QQmlApplicationEngine;

// Private, zero-terminal input for the ordinary client construction path.
QJsonObject clientCheckConfiguration(const QString &directory);
QJsonObject completeClientCheck(QJsonObject modules, QQmlApplicationEngine &engine, const QStringList &warnings);
