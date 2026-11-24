#pragma once
#include "ProcessingChain.h"

namespace noclip
{
struct ComparisonSettings { Parameters parameters; int quality=2; };

// Energy comparison, not LUFS. Only attenuates; both slots see the same source.
class RmsMatcher
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate=rate; power={}; gainDb={}; measured=0;
        energyPole=std::exp(-1/(rate*.4)); gainPole=std::exp(-1/(rate*.5));
    }
    void restartMeasurement() noexcept { power={}; measured=0; }
    std::array<float,2> next(double a,double b,bool observe,bool enabled) noexcept
    {
        if(observe)
        {
            power[0]=energyPole*power[0]+(1-energyPole)*a;
            power[1]=energyPole*power[1]+(1-energyPole)*b;
            measured=std::min(measured+1,static_cast<int>(sampleRate));
        }
        const double quieter=std::min(power[0],power[1]);
        const bool valid=enabled&&ready();
        std::array<float,2> gains;
        for(size_t i=0;i<2;++i)
        {
            const double target=valid?std::clamp(10*std::log10(quieter/power[i]),-12.0,0.0):0;
            gainDb[i]=gainPole*gainDb[i]+(1-gainPole)*target;
            gains[i]=static_cast<float>(std::pow(10.0,gainDb[i]/20));
        }
        return gains;
    }
    bool ready() const noexcept { return measured>=static_cast<int>(sampleRate*.4)&&std::min(power[0],power[1])>1e-8; }
    float attenuation(int slot) const noexcept { return static_cast<float>(-gainDb[static_cast<size_t>(slot)]); }
private:
    std::array<double,2> power {},gainDb {};
    double sampleRate=48000,energyPole=0,gainPole=0;
    int measured=0;
};

class ComparisonEngine
{
public:
    void prepare(double rate,int channels,const std::array<ComparisonSettings,2>&,
                 int selected,const std::array<CurveTable,2>&);
    MeterFrame process(float* const*,int channels,int samples,const std::array<ComparisonSettings,2>&,
                       int selected,bool match,bool hostBypass,const float* const* key=nullptr,int keyChannels=0) noexcept;
    void setTable(int slot,const CurveTable& table) noexcept { engines[static_cast<size_t>(slot)].setTable(table); }
    int latency() const noexcept { return engines[0].latency(); }
    int activeQuality() const noexcept { return engines[blend.current<.5f?0:1].activeQuality(); }
    float sidechainReduction() const noexcept { return engines[blend.current<.5f?0:1].sidechainReduction(); }
    float matchAttenuation() const noexcept { return matcher.attenuation(blend.current<.5f?0:1); }
    bool ready() const noexcept { return matcher.ready()&&matchWarmup==0; }
    bool switching() const noexcept { return warmup>0||blend.left>0; }
private:
    std::array<ProcessingChain,2> engines;
    std::array<std::array<std::array<float,256>,2>,2> work {};
    Smooth blend;
    RmsMatcher matcher;
    double sampleRate=48000;
    int desired=0,warmup=0,matchWarmup=0;
    bool wasMatching=false;
};
}
