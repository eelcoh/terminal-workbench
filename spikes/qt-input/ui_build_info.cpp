#include "ui_build_info.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QSysInfo>

UiBuildInfo::UiBuildInfo(const QJsonObject &configuration, QObject *parent) : QObject(parent)
{
    const auto build=configuration["build_info"].toObject();
    const auto revision=build["revision"].toString();
    const auto kind=build["kind"].toString(QStringLiteral("development checkout"));
    const auto version=build["version"].toString();
    summary_=revision.isEmpty() ? kind : (version.isEmpty() ? QString() : version+QStringLiteral(" · "))+revision.left(12)+(build["source_dirty"].toBool() ? QStringLiteral(" · local changes") : QString());
    // Explicit allowlist: no catalog, terminal snapshot, workspace paths, agent
    // state, selection or process environment is copied into this report.
    QJsonObject report{{"schema",1},{"application","Band"},{"client_kind",kind},
        {"version",version},{"revision",revision},{"source_dirty",build["source_dirty"].toBool()},
        {"qt_version",QString::fromLatin1(qVersion())},{"platform",QSysInfo::kernelType()},
        {"architecture",QSysInfo::currentCpuArchitecture()},
        {"qt_bundled",build["qt_bundled"].toBool()},{"selective_qml",build["selective_qml"].toBool()},
        {"payload_root",build["payload_root"].toString()},{"python",build["python"].toString()},
        {"configured_runtime",configuration["runtime"].toString()},
        {"viewer",configuration["viewer"].toString()},
        {"configuration_file",configuration["preferences"].toString()},
        {"state_directory",configuration["state"].toString()},
        {"window_view_file",configuration["view_state"].toString()}};
    if(build.contains("metadata_error"))report["metadata_error"]=build["metadata_error"].toString();
    diagnostics_=QString::fromUtf8(QJsonDocument(report).toJson());
}

void UiBuildInfo::copyDiagnostics() const
{
    if(auto *clipboard=QGuiApplication::clipboard())clipboard->setText(diagnostics_);
}
