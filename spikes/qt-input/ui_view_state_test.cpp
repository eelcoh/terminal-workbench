#include "ui_view_state.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char *text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static QByteArray read(const QString &path){QFile file(path);check(file.open(QIODevice::ReadOnly),"read fixture");return file.readAll();}
static void put(const QString &path,const QByteArray &bytes){QFile file(path);check(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(),"write fixture");}
static QVariantMap fixture(){
    const QVariantMap theme{{"name","Saved theme"},{"colors","background = #123456\n"},{"source",""},{"attribution",""}};
    return {{"version",1},{"window",QVariantMap{{"width",1240},{"height",760}}},{"workspace","w2"},{"terminal","t2"},{"overview",true},{"agents",false},{"needs",true},{"motion",true},{"overview_y",120.5},
        {"workspaces",QVariantMap{{"w2",QVariantMap{{"focus","t2"},{"x",222.5},{"theme",theme}}}}},
        {"terminals",QVariantMap{{"t2",QVariantMap{{"width",2.0/3.0},{"full",true}}}}}};
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir dir;check(dir.isValid(),"temporary directory");const auto path=dir.filePath("window.json");const auto state=fixture();
    check(UiViewState::valid(state),"typed view fixture");UiViewState fresh(path);check(fresh.initial().isEmpty() && fresh.save(state),"initial atomic save");
    UiViewState restored(path);check(restored.initial()==state,"stable identity and fractional layout roundtrip");
    const auto bytes=read(path);check(restored.save(state) && read(path)==bytes,"unchanged save");
    auto pinned=state;pinned["navigation_pinned"]=true;check(UiViewState::valid(pinned),"optional pin preference accepts old profiles");
    const auto pinnedPath=dir.filePath("pinned.json");UiViewState pinStore(pinnedPath);check(pinStore.save(pinned),"pin preference save");UiViewState pinRestore(pinnedPath);check(pinRestore.initial()==pinned,"pin preference roundtrip");
    pinned["navigation_pinned"]="true";check(!UiViewState::valid(pinned),"pin preference stays typed");
    auto inherited=state;auto spaces=inherited["workspaces"].toMap();auto workspace=spaces["w2"].toMap();workspace["theme_default"]=true;spaces["w2"]=workspace;inherited["workspaces"]=spaces;
    const auto defaultPath=dir.filePath("default-theme.json");UiViewState defaults(defaultPath);check(defaults.save(inherited),"default inheritance marker save");UiViewState defaultRestore(defaultPath);check(defaultRestore.initial()==inherited,"default inheritance survives restart with fallback snapshot");
    workspace["theme_default"]="true";spaces["w2"]=workspace;inherited["workspaces"]=spaces;check(!UiViewState::valid(inherited),"inheritance marker stays typed");
    auto invalid=state;invalid["version"]=2;check(!UiViewState::valid(invalid),"future version rejected");
    invalid=state;invalid["overview"]="true";check(!UiViewState::valid(invalid),"filter booleans stay typed");
    invalid=state;invalid["window"]=QVariantMap{{"width",40000},{"height",760}};check(!UiViewState::valid(invalid),"geometry bounded");
    invalid=state;invalid["terminals"]=QVariantMap{{"t2",QVariantMap{{"width",0},{"full",true}}}};check(!UiViewState::valid(invalid),"width bounded");
    invalid=state;invalid["terminal"]="../socket";check(!UiViewState::valid(invalid),"identity does not become a path");
    invalid=state;invalid["unexpected"]=true;check(!UiViewState::valid(invalid),"unknown field rejected");
    put(path,"{bad");UiViewState broken(path);check(!broken.error().isEmpty() && broken.initial().isEmpty() && !broken.save(state) && read(path)=="{bad","invalid view preserved");
    put(path,QByteArray(128*1024+1,' '));UiViewState huge(path);check(!huge.error().isEmpty() && !huge.save(state),"oversize view preserved");
    put(path,bytes);UiViewState stale(path);put(path,bytes+" ");check(!stale.save(state) && read(path)==bytes+" ","stale view protected");
    const auto blocked=dir.filePath("blocked");put(blocked,"file");UiViewState failed(blocked+"/window.json");check(!failed.save(state) && !failed.error().isEmpty(),"failed save is local");
    UiViewState disposable("");check(!disposable.enabled() && !disposable.save(state),"disposable study does not save views");
    std::puts("PASS: typed bounded Qt view roundtrip, stable IDs, fractional widths/scroll, filters, invalid/oversized/stale preservation and failure isolation");
}
