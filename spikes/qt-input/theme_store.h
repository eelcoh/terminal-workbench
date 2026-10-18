#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
#include <functional>

struct StoredTheme {
    QString id,name,source,attribution;
    QByteArray text;
};
// Immutable color files + one atomic index. Never opens the runtime catalog.
class ThemeStore {
public:
    static constexpr int maxThemes=64,maxBytes=64*1024,maxManifestBytes=512*1024;
    static QString digest(const QByteArray &text);
    static bool load(const QString &directory,QVector<StoredTheme> &themes,QString &error);
    static bool insert(const QString &directory,const StoredTheme &theme,QVector<StoredTheme> &merged,QString &error,const std::function<bool(const QByteArray &,QString &)> &validate);
    static bool save(const QString &directory,const QVector<StoredTheme> &themes,QString &error);
};
