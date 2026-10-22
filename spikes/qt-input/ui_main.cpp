// Qt candidate: client presentation around disposable or explicitly saved sessions.
#include "terminal_item.h"
#include "deployment_check.h"
#include "ui_build_info.h"
#include "ui_agent_info.h"
#include "ui_runtime_info.h"
#include "ui_configuration.h"
#include "ui_workspace_appearance.h"
#include "ui_startup_check.h"
#include "chrome_icon.h"
#include <QFontDatabase>
#include <QClipboard>
#include "ui_session.h"
#include "ui_display_report.h"
#include "ui_themes.h"
#include "ui_fonts.h"
#include "ui_view_state.h"
#include "ui_shortcuts.h"
#include <QDir>
#include <QQmlContext>
#include <QJSValue>
#include "terminal_accessibility.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QQmlError>
#include <cstdio>
#include <memory>

static QVariantList listProperty(QObject *object,const char *name) {
    const auto value=object->property(name);
    return value.metaType()==QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setApplicationName("Terminal Workbench UI Study");
    app.setDesktopFileName("terminal-workbench-ui-study");
    QQuickStyle::setStyle("Basic");
    const int interfaceFont=QFontDatabase::addApplicationFont(":/fonts/adwaita-sans/AdwaitaSans-Regular.ttf");
    const auto interfaceFamilies=QFontDatabase::applicationFontFamilies(interfaceFont);
    if(!interfaceFamilies.isEmpty()) {QFont font(interfaceFamilies.first());font.setPixelSize(14);app.setFont(font);}
    else std::fputs("Qt UI: bundled interface font unavailable; using platform fallback\n",stderr);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    qmlRegisterType<ChromeIcon>("Workbench.Probe",1,0,"ChromeIcon");
    const auto args=app.arguments();
    const bool checking=args.value(1)=="--check-deployment";
    QJsonObject modules,configuration;
    std::unique_ptr<QTemporaryDir> checkDirectory;
    QFile config;
    if(checking) {
        modules=deploymentCheck(args.value(2));
        checkDirectory=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/wb-client-check-XXXXXX");
        if(!checkDirectory->isValid()){std::fputs("Qt UI: cannot create private client check directory\n",stderr);return 1;}
        configuration=clientCheckConfiguration(checkDirectory->path());
    } else {
        if(argc<2)return 2;
        config.setFileName(QString::fromLocal8Bit(argv[1]));if(!config.open(QIODevice::ReadOnly))return 2;
        configuration=QJsonDocument::fromJson(config.readAll()).object();
        if(!configuration["catalog"].isObject() || configuration["state"].toString().isEmpty())return 2;
    }
    if(configuration["installed_preview"].toBool()) {
        app.setApplicationName("Terminal Workbench Preview");
        app.setDesktopFileName("terminal-workbench-qt-preview");
    }
    if(!checking && args.contains("--remove-launch-config")) {
        config.close();
        if(!QFile::remove(config.fileName()))std::fputs("Qt preview: could not remove launch configuration\n",stderr);
    }
    UiBuildInfo buildInfo(configuration);
    UiSession session(configuration);
    UiRuntimeInfo runtimeInfo(configuration["state"].toString(),configuration["runtime"].toString());
    UiViewState view(configuration["view_state"].toString());
#if defined(Q_OS_MACOS)
    constexpr bool apple=true;
#else
    constexpr bool apple=false;
#endif
    UiFonts fonts(configuration["preferences"].toString(),apple,nullptr,configuration["font_overrides"].toString());
    UiShortcuts shortcuts(configuration["preferences"].toString(),apple,nullptr,configuration["shortcut_overrides"].toString());
    if(!shortcuts.error().isEmpty())std::fprintf(stderr,"Qt UI shortcut settings: %s\n",qPrintable(shortcuts.error()));
    if(!fonts.error().isEmpty())std::fprintf(stderr,"Qt UI font settings: %s\n",qPrintable(fonts.error()));
    UiThemes themes(configuration["theme_store"].toString(QDir(configuration["state"].toString()).filePath("theme-imports")),nullptr,nullptr,15000,configuration["theme_directory"].toString());
    UiAppearance appearance(configuration["preferences"].toString(),apple,themes);
    UiConfiguration settings(fonts,shortcuts,!checking,nullptr,&appearance,apple);
    UiAgentInfo agentInfo;
    UiWorkspaceAppearance workspaceAppearance(QDir(configuration["state"].toString()).filePath("qt/workspace-appearance"),!checking);
    qmlRegisterType<TerminalItem>("Workbench.Probe",1,0,"TerminalItem");installTerminalAccessibility();
    QStringList startupWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine,&QQmlEngine::warnings,&app,[&](const QList<QQmlError> &errors){for(const auto &error:errors){if(checking)startupWarnings.append(error.toString());std::fprintf(stderr,"Qt UI: %s\n",qPrintable(error.toString()));}});
    if(checking) {
        engine.setInitialProperties({{"visible",false}});
        if(!args.value(2).isEmpty())engine.setImportPathList({args.value(2),"qrc:/qt-project.org/imports","qrc:/qt/qml"});
    }
    engine.rootContext()->setContextProperty("testFolderDialogs",args.contains("--test-report"));
    engine.rootContext()->setContextProperty("studyBuild",&buildInfo);
    engine.rootContext()->setContextProperty("studyAgentInfo",&agentInfo);
    engine.rootContext()->setContextProperty("studyRuntimeInfo",&runtimeInfo);
    engine.rootContext()->setContextProperty("studyConfiguration",&settings);
    engine.rootContext()->setContextProperty("studyView",&view);
    engine.rootContext()->setContextProperty("studyWorkspaceAppearance",&workspaceAppearance);
    engine.rootContext()->setContextProperty("studyThemes",&themes);
    engine.rootContext()->setContextProperty("studyFonts",&fonts);
    engine.rootContext()->setContextProperty("studyShortcuts",&shortcuts);
    engine.rootContext()->setContextProperty("studySession",&session);engine.load(QUrl("qrc:/Ui.qml"));
    if(checking) {
        const auto report=completeClientCheck(modules,engine,startupWarnings);
        std::fputs(QJsonDocument(report).toJson().constData(),stdout);
        return report["qml_ok"].toBool()?0:1;
    }
    if(engine.rootObjects().isEmpty()){std::fputs("Qt UI: no root window was created\n",stderr);return 1;}
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());if(!window){std::fputs("Qt UI: root object is not a window\n",stderr);return 1;}
    const auto displayReport=qEnvironmentVariable("WORKBENCH_DISPLAY_REPORT");
    if(!displayReport.isEmpty()) {
        new UiDisplayReport(window,displayReport);
        std::fprintf(stderr,"Qt display report: %s\n",qPrintable(QFileInfo(displayReport).absoluteFilePath()));
    }
    QObject::connect(&app,&QCoreApplication::aboutToQuit,window,[window]{QMetaObject::invokeMethod(window,"persistView");});
    // Explicit test-only snapshots; no user terminal content is recorded by default.
    const int reportIndex=args.indexOf("--test-report");QTimer reporter;
    if(reportIndex>=0 && reportIndex+1<args.size()) {
        const auto path=args[reportIndex+1];reporter.setInterval(80);
        QObject::connect(&reporter,&QTimer::timeout,&app,[&,path] {
            QJsonArray panes;
            const auto items=listProperty(window,"terminalItems"),labels=listProperty(window,"overviewLabels");
            for(const auto &value:items) {
                auto *item=qobject_cast<TerminalItem *>(value.value<QObject *>());if(!item)continue;
                const auto *caption=panes.size()<labels.size() ? qobject_cast<QQuickItem *>(labels[panes.size()].value<QObject *>()) : nullptr;
                const auto life=item->parentItem()->property("lifecycle").toMap();
                const auto actionPoint=[item](const QString &name){const auto *button=item->parentItem()->findChild<QQuickItem *>(name);if(!button)return QJsonArray();const auto p=button->mapToScene(QPointF(button->width()/2,button->height()/2));return QJsonArray{p.x(),p.y()};};
                const auto *exitedClose=item->parentItem()->findChild<QQuickItem *>("terminalCloseExited"+QString::number(panes.size()));
                const auto size=item->terminalGridSize();const auto position=item->mapToScene(QPointF(item->width()/2,item->height()/2));
                const auto cell=item->cellSize();
                const auto bounds=item->parentItem()->mapRectToScene(item->parentItem()->boundingRect());
                const auto *layoutButton=item->parentItem()->findChild<QQuickItem *>("paneLayoutButton");
                const auto layoutPoint=layoutButton->mapToScene(QPointF(layoutButton->width()/2,layoutButton->height()/2));
                const QSize geometry(qBound(1,int((item->width()-16)/cell.width()),1000),qBound(1,int((item->height()-16)/cell.height()),1000));
                panes.append(QJsonObject{{"ready",item->ready()},{"status",item->status()},{"text",item->viewportText()},
                    {"cursor",item->cursorPresentation()},
                    {"attachment",item->attachment()},{"attachment_attempts",double(item->connectionAttempts())},{"lifecycle",QJsonObject::fromVariantMap(life)},
                    {"instance",QString::number(quintptr(item),16)},
                    {"primary_action",actionPoint("terminalPrimary"+QString::number(panes.size()))},
                    {"check_action",actionPoint("terminalCheck"+QString::number(panes.size()))},
                    {"exited_close_action",actionPoint("terminalCloseExited"+QString::number(panes.size()))},
                    {"exited_close_visible",exitedClose && exitedClose->isVisible()},
                    {"reconnect_action",actionPoint("terminalReconnect"+QString::number(panes.size()))},
                    {"caption",caption ? caption->property("text").toString() : QString()},{"caption_visible",caption && caption->isVisible()},
                    {"facts",QJsonObject::fromVariantMap(item->property("runtimeFacts").toMap())},{"visible",item->parentItem()->isVisible()},
                    {"selection",item->accessibleSnapshot().selectionStart>=0},
                    {"identity",item->property("stableIdentity").toString()},{"layout_button",QJsonArray{layoutPoint.x(),layoutPoint.y()}},
                    {"workspace",item->property("workspaceIndex").toInt()},{"ordinal",item->property("orderInWorkspace").toInt()},
                    {"theme",item->themeIndex()},{"background",item->backgroundColor().name()},
                    {"sample_green",item->cellForeground(1,2).name()},{"sample_red",item->cellForeground(1,0).name()},{"sample_rgb",item->cellForeground(1,1).name()},
                    {"font",QJsonObject::fromVariantMap(item->textFont())},{"columns",size.width()},{"rows",size.height()},{"geometry_ready",size==geometry},{"focus",item->hasActiveFocus()},
                    {"left",bounds.left()},{"top",bounds.top()},{"right",bounds.right()},{"bottom",bounds.bottom()},
                    {"x",position.x()},{"y",position.y()},{"rasterized_rows",double(item->rasterizedRows())}});
            }
            const auto *searchPopup=window->findChild<QObject *>("terminalSearchPopup");
            const auto *searchController=searchPopup ? searchPopup->property("controller").value<QObject *>() : nullptr;
            const auto *createButton=window->findChild<QQuickItem *>("newWorkspaceButton");
            const auto createPoint=createButton->mapToScene(QPointF(createButton->width()/2,createButton->height()/2));
            const auto center=[window](const char *name){const QQuickItem *item=window->findChild<QQuickItem *>(name);
                if(!item) { QList<QQuickItem *> items{window->contentItem()};while(!items.isEmpty()) {auto *candidate=items.takeLast();if(candidate->objectName()==QLatin1String(name)){item=candidate;break;}items.append(candidate->childItems());} }
                if(!item)return QJsonArray();
                const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));return QJsonArray{point.x(),point.y()};};
            const auto box=[window](const char *name){const auto *item=window->findChild<QQuickItem *>(name);const auto rect=item->mapRectToScene(item->boundingRect());return QJsonArray{rect.left(),rect.top(),rect.right(),rect.bottom()};};
            QJsonArray folderChoices;QJsonArray folderAccept;QJsonArray folderList;QJsonArray navigationAgents;bool folderMoving=false;
            // Observe the installed fallback controls; no test actions enter production QML.
            QList<QQuickItem *> pending{window->contentItem()};
            while(!pending.isEmpty()) {
                auto *item=pending.takeLast();
                if(!item->isVisible())continue;
                pending.append(item->childItems());
                const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
                bool unclipped=true;
                for(auto *parent=item->parentItem();parent;parent=parent->parentItem())
                    if(parent->clip() && !parent->mapRectToScene(parent->boundingRect()).contains(point)) {unclipped=false;break;}
                if(unclipped && item->objectName().startsWith("navigationAgent") && item->objectName()!="navigationAgents")navigationAgents.append(item->property("hint").toString());
                if(item->objectName()=="folderDialogListView") {folderList=QJsonArray{point.x(),point.y()};folderMoving=item->property("moving").toBool();}
                if(unclipped && item->objectName().startsWith("folderDialogDelegate"))folderChoices.append(QJsonObject{{"name",item->property("fileName").toString()},{"selected",item->property("highlighted").toBool()},{"point",QJsonArray{point.x(),point.y()}}});
                if(item->property("text").toString()=="Choose folder" && item->inherits("QQuickAbstractButton"))folderAccept=QJsonArray{point.x(),point.y()};
            }
            const QJsonObject data{{"configuration_open",window->property("configurationOpen").toBool()},
                {"creation_directory",settings.workspaceDirectory()},{"creation_width",settings.terminalWidth()},{"pane_widths",QJsonArray::fromVariantList(listProperty(window,"widths"))},{"workspace_appearance_error",workspaceAppearance.error()},{"workspace_appearance_directory",workspaceAppearance.directory()},{"configuration_error",settings.error()},{"configuration_status",settings.status()},
                {"configuration_menu",center("configurationMenuItem")},{"configuration_reload",center("configurationReload")},{"configuration_close",center("configurationClose")},
                {"runtime_info_open",window->property("runtimeInfoOpen").toBool()},{"runtime_info_target",window->property("runtimeInfoTarget").toString()},
                {"runtime_info_report",QJsonObject::fromVariantMap(runtimeInfo.report())},
                {"runtime_info_ownership",window->findChild<QObject *>("terminalInformation")->property("ownership").toString()},
                {"runtime_info_clipboard_matches",!runtimeInfo.diagnostics().isEmpty() && QGuiApplication::clipboard()->text()==runtimeInfo.diagnostics()},
                {"runtime_info_copy",center("terminalInfoCopy")},{"runtime_info_refresh",center("terminalInfoRefresh")},{"runtime_info_expand",center("terminalInfoExpand")},
                {"runtime_info_menu",center("terminalInfoMenuItem")},{"agent_info_runtime",center("agentInfoRuntime")},
                {"agent_info_open",window->property("agentInfoOpen").toBool()},{"agent_info_target",window->property("agentInfoTarget").toString()},
                {"agent_info_details",window->findChild<QObject *>("agentInformation") ? QJsonObject::fromVariantMap(window->findChild<QObject *>("agentInformation")->property("information").toMap()):QJsonObject()},
                {"agent_info_copy",center("agentInfoCopy")},{"agent_info_menu",center("agentInfoMenuItem")},{"navigation_agent_info",center("navigationAgentInfo0")},
                {"agent_launcher_clipboard_matches",QStringList{"wb-codex","wb-claude","wb-copilot","wb-pi"}.contains(QGuiApplication::clipboard()->text())},
                {"about_open",window->property("aboutOpen").toBool()},
                {"about_menu",center("aboutMenuItem")},{"about_copy",center("aboutCopy")},{"about_details",center("aboutDetails")},{"about_close",center("aboutClose")},
                {"about_clipboard_matches",window->property("aboutOpen").toBool() && QGuiApplication::clipboard()->text()==buildInfo.diagnostics()},
                {"about_summary",buildInfo.summary()},{"about_details_ready",window->property("aboutDetailsReady").toBool()},
                {"shortcut_sequences",QJsonObject::fromVariantMap(shortcuts.sequences())},
                {"shortcut_error",shortcuts.error()},{"shortcut_labels",QJsonObject::fromVariantMap(shortcuts.labels())},
                {"font_settings_open",window->property("fontSettingsOpen").toBool()},
                {"forwarding_armed",window->property("forwardingArmed").toBool()},{"forward_menu",center("forwardKeyMenuItem")},{"forward_cancel",center("cancelForwardKey")},
                {"shortcut_settings_open",window->property("shortcutSettingsOpen").toBool()},
                {"shortcut_recording",window->property("shortcutRecording").toBool()},
                {"shortcut_edit_error",shortcuts.editError()},{"shortcut_draft_labels",QJsonObject::fromVariantMap(shortcuts.draftLabels())},
                {"shortcut_settings_menu",center("shortcutSettingsMenuItem")},{"shortcut_record",center("shortcutRecord")},
                {"shortcut_apply",center("shortcutApply")},{"shortcut_cancel",center("shortcutCancel")},
                {"shortcut_clear",center("shortcutClear")},{"shortcut_reset",center("shortcutReset")},
                {"font_edit_error",fonts.editError()},{"font_preferences",QJsonObject::fromVariantMap(fonts.current())},
                {"font_defaults",QJsonObject::fromVariantMap(fonts.defaults())},
                {"font_menu_item",center("fontSettingsMenuItem")},{"font_plus",center("fontPlus")},{"font_minus",center("fontMinus")},
                {"font_fira",center("fontFira")},{"font_jetbrains",center("fontJetBrains")},{"font_apply",center("fontApply")},{"font_defaults_button",center("fontDefaults")},
                {"commands_open",window->property("commandsOpen").toBool()},
                {"command_first_row",center("commandRow0")},{"reduced_motion",window->property("reducedMotion").toBool()},
                {"command_current",window->property("commandCurrent").toInt()},
                {"command_search_focus",window->property("commandSearchFocus").toBool()},
                {"command_matches",QJsonArray::fromVariantList(listProperty(window,"commandMatches"))},
                {"commands_menu_item",center("commandsMenuItem")},
                {"confirmation_cancel_focus",window->findChild<QQuickItem *>("cancelTerminalAction")->hasActiveFocus()},{"terminal_action_open",window->property("terminalActionOpen").toBool()},
                {"terminal_action_ready",window->findChild<QObject *>("terminalActionDialog")->property("opened").toBool()},
                {"terminal_action_kind",window->property("actionKind").toString()},{"terminal_action_target",window->property("actionTarget").toString()},{"workspace_removal_context",window->property("workspaceRemovalContext").toString()},
                {"remove_workspace_0",center("removeWorkspace0")},{"remove_workspace_1",center("removeWorkspace1")},{"remove_workspace_3",center("removeWorkspace3")},
                {"confirm_terminal_action_enabled",window->findChild<QQuickItem *>("confirmTerminalAction")->isEnabled()},{"session_stop_item",center("sessionStopMenuItem")},{"session_stop_ids",QJsonArray::fromVariantList(listProperty(window,"stopTargets"))},{"session_stop_valid",window->property("stopTargetsValid").toBool()},
                {"terminal_close_item",center("terminalCloseMenuItem")},{"confirm_terminal_action",center("confirmTerminalAction")},{"cancel_terminal_action",center("cancelTerminalAction")},
                {"view_enabled",view.enabled()},{"view_error",view.error()},{"window_width",window->width()},{"interface_font",app.font().family()},
                {"chrome_boxes",QJsonObject{{"workspace",box("workspaceButton")},{"indicators",box("indicatorRow")},{"count",box("focusedCount")},{"add",box("newTerminalButton")},{"overview",box("overviewButton")}}},
                {"workspaces_button",center("workspacesButton")},
                {"workspace",window->property("workspace").toInt()},
                {"move_notice",window->property("moveNotice").toString()},
                {"agents_only",window->property("agentsOnly").toBool()},{"needs_only",window->property("needsOnly").toBool()},
                {"overview_search",center("overviewSearch")},{"overview_search_focus",window->property("overviewSearchFocused").toBool()},
                {"overview_columns",window->property("overviewColumns").toInt()},{"overview_clear_filters",center("overviewClearFilters")},{"overview_matches",window->property("overviewMatches").toInt()},{"waiting_count",window->property("waitingCount").toInt()},
                {"waiting_identities",QJsonArray::fromVariantList(listProperty(window,"waitingIdentities"))},{"workspace_waiting_counts",QJsonArray::fromVariantList(listProperty(window,"workspaceWaitingCounts"))},{"overview_groups",QJsonArray::fromVariantList(listProperty(window,"overviewGroupTops"))},
                {"agent_filter_button",center("overviewAgentFilter")},{"state_filter_button",center("overviewStateFilter")},{"agents_button",center("agentsButton")},
                {"selected",window->property("selected").toInt()},{"overview",window->property("overview").toBool()},
                {"drop_allowed",window->property("dropAllowed").toBool()},
                {"drag_slot",window->property("dragSlot").toInt()},{"drop_space",window->property("dropSpace").toInt()},{"drop_position",window->property("dropPosition").toInt()},
                {"content_y",window->findChild<QQuickItem *>("deck")->property("contentY").toDouble()},
                {"content_x",window->findChild<QQuickItem *>("deck")->property("contentX").toDouble()},
                {"navigation_default",window->property("navigationOverride").isNull()},{"motion_default",window->property("motionOverride").isNull()},
                {"managed_navigation_pinned",settings.navigationPinned()},{"managed_reduced_motion",settings.reducedMotion()},
                {"configuration_view_defaults",center("configurationViewDefaults")},
                {"catalog_notice",session.catalogNotice()},{"busy",session.busy()},{"error",session.error()},
                {"workspace_count",listProperty(window,"spaces").size()},
                {"appearance_open",window->property("appearanceOpen").toBool()},
                {"application_menu_open",window->property("applicationMenuOpen").toBool()},
                {"layout_menu_open",window->property("layoutMenuOpen").toBool()},
                {"move_right_item",center("moveRightMenuItem")},
                {"theme_search_focus",window->property("themeSearchFocus").toBool()},
                {"theme_apply_button",center("workspaceThemeApply")},{"theme_default_button",center("workspaceThemeDefault")},{"theme_default_index",settings.themeIndex()},{"workspace_themes",QJsonArray::fromVariantList(listProperty(window,"workspaceThemes"))},{"theme_search_field",center("themeSearch")},{"theme_help_button",center("themeHelpButton")},{"theme_help_open",window->property("themeHelpOpen").toBool()},
                {"theme_matches",window->property("themeMatchCount").toInt()},
                {"theme_directory",themes.themesPath()},{"theme_directory_status",themes.directoryStatus()},{"theme_catalog_count",themes.catalog().size()},
                {"chrome_background",window->color().name()},
                {"application_menu_button",center("applicationMenuButton")},{"appearance_menu_item",center("appearanceMenuItem")},
                {"rename_workspace_item",center("renameWorkspaceMenuItem")},{"workspace_folder_item",center("workspaceFolderMenuItem")},
                {"workspace_form_mode",window->property("workspaceFormMode").toString()},
                {"workspace_names",QJsonArray::fromVariantList(listProperty(window,"spaces"))},
                {"workspace_directories",QJsonArray::fromVariantList(listProperty(window,"directories"))},
                {"folder_focus",window->property("folderFocused").toBool()},
                {"folder_browser_open",window->property("folderBrowserOpen").toBool()},
                {"folder_browser_location",window->property("folderBrowserLocation").toUrl().toLocalFile()},
                {"folder_choices",folderChoices},{"folder_accept",folderAccept},{"folder_list",folderList},{"folder_list_moving",folderMoving},
                {"folder_browser_selection",window->property("folderBrowserSelection").toUrl().toLocalFile()},
                {"browse_folder_button",center("browseWorkspaceFolderButton")},
                {"name_input",window->findChild<QQuickItem *>("workspaceNameInput")->property("text").toString()},
                {"folder_input",window->findChild<QQuickItem *>("workspaceFolderInput")->property("text").toString()},
                {"form_submit_enabled",window->findChild<QQuickItem *>("submitWorkspaceButton")->isEnabled()},
                {"creation_open",window->property("creationOpen").toBool()},
                {"navigation_pinned",window->property("navigationPinned").toBool()},{"navigation_docked",window->property("navigationDocked").toBool()},
                {"navigation_agent_labels",navigationAgents},{"navigation_section",window->property("navigationSection").toString()},{"navigation_pin",center("navigationPin")},{"navigation_workspaces",center("navigationWorkspaces")},
                {"navigation_search",center("navigationAgentSearch")},{"navigation_search_focus",window->property("navigationSearchFocused").toBool()},
                {"navigation_matches",QJsonArray::fromVariantList(listProperty(window,"navigationMatches"))},{"navigation_needs_only",window->property("navigationNeedsOnly").toBool()},
                {"navigation_needs_filter",center("navigationNeedsFilter")},{"navigation_clear_filters",center("navigationClearFilters")},
                {"navigation_agents",center("navigationAgents")},{"navigation_agent_indices",QJsonArray::fromVariantList(listProperty(window,"agentIndices"))},
                {"navigation_first_agent",center("navigationAgent0")},{"navigation_workspace_1",center("navigationWorkspace1")},
                {"drawer_open",window->property("drawerOpen").toBool()},{"name_focus",window->property("nameFocused").toBool()},
                {"new_workspace_button",QJsonArray{createPoint.x(),createPoint.y()}},
                {"terminal_search_match_case",center("terminalSearchMatchCase")},{"terminal_search_whole_word",center("terminalSearchWholeWord")},
                {"terminal_search_open",window->property("terminalSearchOpen").toBool()},{"terminal_search_focus",window->property("terminalSearchFocused").toBool()},
                {"terminal_search_state",searchController ? QJsonObject::fromVariantMap(searchController->property("state").toMap()) : QJsonObject()},
                {"terminal_search_next",center("terminalSearchNext")},{"terminal_search_close",center("terminalSearchClose")},
                {"panes",panes},{"platform",QGuiApplication::platformName()}};
            QSaveFile report(path);if(report.open(QIODevice::WriteOnly)){report.write(QJsonDocument(data).toJson());report.commit();}
            // Inject only through the window so tests exercise QML gesture arbitration too.
            QFile wheelRequest(path+".wheel");
            if(wheelRequest.open(QIODevice::ReadOnly)) {
                const auto request=QJsonDocument::fromJson(wheelRequest.read(4096)).object();
                const int index=request["pane"].toInt();
                if(index>=0 && index<items.size()) {
                    auto *item=qobject_cast<TerminalItem *>(items[index].value<QObject *>());
                    if(item) {
                        auto *source=request["deck"].toBool()?window->findChild<QQuickItem *>("deck"):item;
                        const auto local=source->mapToScene(QPointF(source->width()/2,source->height()/2));
                        const auto phase=[&](Qt::ScrollPhase value){QWheelEvent event(local,window->mapToGlobal(local),QPoint(),QPoint(),Qt::NoButton,Qt::NoModifier,value,false);QCoreApplication::sendEvent(window,&event);};
                        if(request["begin"].toBool())phase(Qt::ScrollBegin);
                        const auto delta=[&request](const char *key){const auto values=request[key].toArray();return QPoint(qBound(-120,values[0].toInt(),120),qBound(-120,values[1].toInt(),120));};
                        for(int count=qBound(1,request["count"].toInt(1),200);count>0;--count) {
                            QWheelEvent event(local,window->mapToGlobal(local),delta("pixel"),delta("angle"),Qt::NoButton,Qt::NoModifier,Qt::ScrollUpdate,request["inverted"].toBool());
                            QCoreApplication::sendEvent(window,&event);
                        }
                        if(request["end"].toBool())phase(Qt::ScrollEnd);
                    }
                }
                wheelRequest.close();QFile::remove(path+".wheel");
            }
            if(QFile::exists(path+".capture")) {
                const auto image=window->grabWindow();
                if(image.save(path+".png")) {
                    QJsonArray cursors;
                    for(const auto &value:items) {
                        auto *item=qobject_cast<TerminalItem *>(value.value<QObject *>());if(!item)continue;
                        const auto rect=item->mapRectToScene(item->caret());
                        const qreal sx=qreal(image.width())/window->width(),sy=qreal(image.height())/window->height();
                        const QRect pixels=QRect(qRound(rect.x()*sx),qRound(rect.y()*sy),qRound(rect.width()*sx),qRound(rect.height()*sy)).intersected(image.rect());
                        int cursor=0,background=0;
                        for(int y=pixels.top();y<=pixels.bottom();++y)for(int x=pixels.left();x<=pixels.right();++x) {
                            const auto color=image.pixelColor(x,y);
                            if(color==item->cursorColor())++cursor;
                            if(color==item->backgroundColor())++background;
                        }
                        cursors.append(QJsonObject{{"cursor_pixels",cursor},{"background_pixels",background},{"area",pixels.width()*pixels.height()}});
                    }
                    QSaveFile metrics(path+".cursor.json");if(metrics.open(QIODevice::WriteOnly)){metrics.write(QJsonDocument(cursors).toJson());metrics.commit();}
                    QFile::remove(path+".capture");
                }
            }
        });reporter.start();
    }
    return app.exec();
}
