#pragma once
#include <QString>
#include <QVariantList>
// Apps from the managed file: [app.ID] sections, with platform overrides.
struct AppDefinitions {
    QString error;
    QVariantList apps; // {id, name, command, directory, install, setup}; directory as written, resolved by the service
    QVariantList hosts; // {id, name, destination, directory, theme}: [host.ID] sections, see docs/hosts.md
    static AppDefinitions read(const QString &path,bool apple);
};
