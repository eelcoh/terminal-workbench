#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>

QStringList deploymentLoadedImages();
QJsonObject deploymentCheck(const QString &qmlRoot = {});
