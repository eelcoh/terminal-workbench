#pragma once
#include <QPoint>
#include <Qt>
#include <algorithm>
#include <cmath>

// Qt already applies the system's natural-scroll policy to its delivered deltas.
// Preserve sub-row motion; Ghostty takes signed whole rows (negative is older).
class TerminalScroll {
public:
    struct Result {int rows=0;bool handled=false;};
    void reset(){remainder_=0;source_=0;}
    Result consume(QPoint pixels,QPoint angles,double rowHeight,int linesPerNotch,bool reliablePixels,Qt::ScrollPhase phase){
        if(phase==Qt::ScrollBegin)reset();
        const bool precise=reliablePixels && !pixels.isNull();
        const auto delta=precise?pixels:angles;
        if(delta.isNull()){if(phase==Qt::ScrollEnd)reset();return {0,phase!=Qt::NoScrollPhase};}
        if(!delta.y() || std::abs(double(delta.x()))>std::abs(double(delta.y())))return {};
        const int source=precise?1:2;
        if(source_!=source){reset();source_=source;}
        if(!std::isfinite(rowHeight) || rowHeight<=0)return {0,true};
        remainder_+=precise ? -double(delta.y())/rowHeight : -double(delta.y())*std::max(0,linesPerNotch)/120.0;
        const auto whole=std::trunc(remainder_);remainder_-=whole;
        const int rows=int(std::clamp(whole,-1000.0,1000.0));
        if(phase==Qt::ScrollEnd)reset();
        return {rows,true};
    }
private:
    double remainder_=0;int source_=0;
};
