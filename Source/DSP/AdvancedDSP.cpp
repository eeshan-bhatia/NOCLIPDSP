#include "AdvancedDSP.h"

namespace noclip
{
namespace {
float safe(float v, float lo, float hi, float fallback=0) noexcept { return std::isfinite(v)?std::clamp(v,lo,hi):fallback; }
float gain(float v) noexcept { return std::pow(10.0f, v*.05f); }
}
void AdvancedDSP::prepare(double rate, AdvancedParameters p)
{
    if(!std::isfinite(rate)) rate=48000;
    sampleRate=std::clamp(rate, 1000.0, 1536000.0);
    for(auto& c:controls) c.prepare(sampleRate,0);
    for(auto& band:bands) for(auto& c:band) c.prepare(sampleRate,0);
    tableBlend.prepare(sampleRate,1);
    setParameters(p);
    for(auto& c:controls) c.prepare(sampleRate,c.target);
    for(auto& band:bands) for(auto& c:band) c.prepare(sampleRate,c.target);
    for(auto& c:crossovers) c.reset();
    for(auto& c:bassClean) c.reset();
    for(auto& c:bassDirty) c.reset();
    dcX={}; dcY={}; fast={}; slow={};
    dcPole=std::exp(-2*std::numbers::pi*10/sampleRate);
    fastPole=std::exp(-1/(sampleRate*.001)); slowPole=std::exp(-1/(sampleRate*.03));
}
void AdvancedDSP::setParameters(AdvancedParameters p) noexcept
{
    controls[custom].set(p.custom?1.0f:0.0f); controls[dc].set(p.dc?1.0f:0.0f);
    controls[bias].set(safe(p.bias,-.5f,.5f)); controls[asym].set(safe(p.asymmetry,-1,1));
    controls[morph].set(safe(p.morph,0,1));
    p.routing=std::clamp(p.routing,0,2);
    for(int i=0;i<3;++i) controls[full+i].set(p.routing==i?1.0f:0.0f);
    const auto low=safe(p.lowHz,40,1000,120), high=safe(p.highHz,1200,12000,2500);
    controls[lowG].set(static_cast<float>(LinkwitzRiley::coefficient(low,sampleRate)));
    controls[highG].set(static_cast<float>(LinkwitzRiley::coefficient(high,sampleRate)));
    controls[splitG].set(static_cast<float>(LinkwitzRiley::coefficient(safe(p.splitHz,40,400,120),sampleRate)));
    controls[sub].set(safe(p.sub,0,1,1)); controls[harmonics].set(safe(p.harmonics,0,1,.5f));
    controls[bassDrive].set(gain(safe(p.bassDrive,0,36))); controls[punch].set(safe(p.punch,0,1));
    controls[bassOutput].set(gain(safe(p.bassOutput,-24,12)));
    for(size_t i=0;i<3;++i)
    {
        bands[i][0].set(gain(safe(p.bands[i].drive,0,36)));
        bands[i][1].set(safe(p.bands[i].shape,0,1));
        bands[i][2].set(gain(safe(p.bands[i].gain,-24,12)));
        bands[i][3].set(safe(p.bands[i].mix,0,1,1));
        bands[i][4].set(p.bands[i].enabled?1.0f:0.0f);
    }
}
void AdvancedDSP::setTable(const CurveTable& next, bool immediate) noexcept
{
    if(immediate) { table=previous=next; tableBlend.prepare(sampleRate,1); return; }
    for(size_t i=0;i<CurveTable::size;++i)
    {
        previous.a[i] += tableBlend.current*(table.a[i]-previous.a[i]);
        previous.b[i] += tableBlend.current*(table.b[i]-previous.b[i]);
    }
    table=next; tableBlend.prepare(sampleRate,0); tableBlend.set(1);
}
void AdvancedDSP::advance(double linkedAmplitude) noexcept
{
    for(size_t i=0;i<controls.size();++i) v[i]=controls[i].next();
    for(size_t i=0;i<3;++i) for(size_t j=0;j<5;++j) bandValues[i][j]=bands[i][j].next();
    tableBlend.next();
    fast[0]=fastPole*fast[0]+(1-fastPole)*linkedAmplitude;
    slow[0]=slowPole*slow[0]+(1-slowPole)*linkedAmplitude;
}
double AdvancedDSP::transfer(double x, float t, float s, float k,
    const std::array<float, curveCount>& weights,float sum) const noexcept
{
    x += v[bias]*t;
    x *= x>=0 ? 1+.75*v[asym] : 1-.75*v[asym];
    double classic=0;
    if(v[custom]<1)
        for(int j=0;j<curveCount;++j) if(weights[j]>0)
            classic+=weights[j]*clip(x,t,s,k,static_cast<Curve>(j))/std::max(1.0e-6f,sum);
    if(v[custom]<=0) return classic;
    const auto evaluate=[&](const CurveTable& c)
    { return CurveTable::evaluate(c.a,x/t)*(1-v[morph])+CurveTable::evaluate(c.b,x/t)*v[morph]; };
    const double shaped=t*(evaluate(previous)*(1-tableBlend.current)+evaluate(table)*tableBlend.current);
    return classic*(1-v[custom])+v[custom]*shaped;
}
std::array<double,3> AdvancedDSP::process(int channel,double dry,float d,float t,float s,float k,
    const std::array<float,curveCount>& weights,float sum,float driveRelief) noexcept
{
    const auto ch=static_cast<size_t>(channel);
    double wet=0, aligned=0, activity=0;
    const double relievedDrive=std::max(1.0,static_cast<double>(d)*driveRelief);
    if(v[full]>0) { wet=v[full]*transfer(dry*relievedDrive,t,s,k,weights,sum); aligned=v[full]*dry; if(std::abs(dry*relievedDrive)>t) activity=v[full]; }
    if(v[multi]>0)
    {
        const auto parts=crossovers[ch].process(dry,v[lowG],v[highG]);
        double processed=0;
        for(size_t i=0;i<3;++i)
        {
            const auto& b=bandValues[i];
            const double bandDrive=std::max(1.0,static_cast<double>(d)*b[0]*driveRelief);
            const double shaped= b[4]>0 ? transfer(parts[i]*bandDrive,t,b[1],k,weights,sum) : parts[i];
            const double mixed=(parts[i]*(1-b[3])+shaped*b[3])*b[2];
            processed+=parts[i]*(1-b[4])+mixed*b[4];
            if(std::abs(parts[i]*bandDrive)>t) activity=std::max(activity,static_cast<double>(v[multi]*b[4]));
        }
        wet+=v[multi]*processed; aligned+=v[multi]*(parts[0]+parts[1]+parts[2]);
    }
    if(v[bass]>0)
    {
        const double attack=std::clamp((fast[0]-slow[0])/(fast[0]+1e-9),0.0,1.0);
        const double bassGain=1+(v[bassDrive]-1)*(1-v[punch]*attack);
        const auto clean=bassClean[ch].process(dry,v[splitG]);
        const double dirtyDrive=std::max(1.0,static_cast<double>(d)*bassGain*driveRelief);
        const auto dirty=bassDirty[ch].process(transfer(dry*dirtyDrive,t,s,k,weights,sum),v[splitG]);
        wet+=v[bass]*v[bassOutput]*(clean[0]*v[sub]+dirty[0]*(1-v[sub])
                                  +clean[1]*(1-v[harmonics])+dirty[1]*v[harmonics]);
        aligned+=v[bass]*(clean[0]+clean[1]);
        if(std::abs(dry*dirtyDrive)>t) activity=std::max(activity,static_cast<double>(v[bass]));
    }
    const double filtered=(1+dcPole)*.5*(wet-dcX[ch])+dcPole*dcY[ch];
    dcX[ch]=wet; dcY[ch]=filtered;
    wet=wet*(1-v[dc])+filtered*v[dc];
    return {aligned,wet,activity};
}
}
