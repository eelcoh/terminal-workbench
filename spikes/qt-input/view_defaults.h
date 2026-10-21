#pragma once
#include <QString>
// Managed client presentation defaults. Saved window choices remain independent.
struct ViewDefaults {
    QString error;
    bool navigationPinned=false,reducedMotion=false;
    static ViewDefaults read(const QString &path,bool apple);
};
