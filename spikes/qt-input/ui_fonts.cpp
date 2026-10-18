#include "ui_fonts.h"
#include "font_bridge.h"
#include <QFile>
#include <algorithm>
UiFonts::UiFonts(const QString &path,bool apple,QObject *parent):QObject(parent)
{
    int family=0,size=16;char error[256]={0};const auto encoded=QFile::encodeName(path);
    if(!workbench_font_defaults(path.isEmpty()?nullptr:encoded.constData(),apple,&family,&size,error,sizeof(error)))error_="Using built-in fonts. "+QString::fromUtf8(error);
    defaults_={{"family",family==1?"Fira Code":"JetBrains Mono"},{"size",size}};
    current_=defaults_;original_=current_;
}
bool UiFonts::preview(const QString &family,int size)
{
    if((family!="Fira Code" && family!="JetBrains Mono") || size<10 || size>32)return false;
    const QVariantMap next{{"family",family},{"size",size}};
    if(current_!=next){current_=next;Q_EMIT changed();}return true;
}
void UiFonts::cancel(){preview(original_["family"].toString(),original_["size"].toInt());}
void UiFonts::useDefaults(){preview(defaults_["family"].toString(),defaults_["size"].toInt());}
void UiFonts::step(int direction){preview(current_["family"].toString(),std::clamp(current_["size"].toInt()+(direction>0?1:direction<0?-1:0),10,32));}
void UiFonts::resetSize(){preview(current_["family"].toString(),defaults_["size"].toInt());}
