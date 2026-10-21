#pragma once
#include <QString>
struct CreationDefaults {
    QString directory,error;
    double terminalWidth=0.5;
    static CreationDefaults read(const QString &path,bool apple);
};
