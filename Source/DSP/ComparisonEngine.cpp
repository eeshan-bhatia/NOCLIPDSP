#include "ComparisonEngine.h"

namespace noclip
{
void ComparisonEngine::prepare(double rate,int channels,const std::array<ComparisonSettings,2>& settings,
                               int selected,const std::array<CurveTable,2>& curves)
{
    sampleRate=std::isfinite(rate)?std::clamp(rate,8000.0,192000.0):48000;
    for(size_t i=0;i<2;++i) engines[i].prepare(sampleRate,channels,settings[i].parameters,settings[i].quality,curves[i]);
    desired=std::clamp(selected,0,1); blend.prepare(sampleRate,static_cast<float>(desired));
    matcher.prepare(sampleRate); warmup=matchWarmup=0; wasMatching=false;
}
MeterFrame ComparisonEngine::process(float* const* channels,int channelCount,int samples,
    const std::array<ComparisonSettings,2>& settings,int selected,bool match,bool hostBypass) noexcept
{
    MeterFrame meters;
    if(channelCount<1||channelCount>2||samples<=0) return meters;
    selected=std::clamp(selected,0,1); match=match&&!hostBypass;
    if(selected!=desired)
    {
        desired=selected;
        if(blend.left>0||wasMatching) { warmup=0; blend.set(static_cast<float>(desired)); }
        else if(blend.current==static_cast<float>(desired)) warmup=0;
        else warmup=static_cast<int>(sampleRate*.12);
    }
    if(match&&!wasMatching) { matchWarmup=static_cast<int>(sampleRate*.12); matcher.restartMeasurement(); }
    wasMatching=match;
    double energy=0,activity=0;
    for(int offset=0;offset<samples;offset+=256)
    {
        const int n=std::min(256,samples-offset);
        const bool both=match||warmup>0||blend.left>0;
        const int audible=blend.current<.5f?0:1;
        std::array<float,2> clipped {};
        for(int ch=0;ch<channelCount;++ch) for(int i=0;i<n;++i)
        {
            const float raw=channels[ch][offset+i];
            const float x=std::isfinite(raw)?raw:0;
            meters.inputPeak=std::max(meters.inputPeak,std::abs(x));
            work[0][ch][i]=work[1][ch][i]=x;
        }
        for(int slot=0;slot<2;++slot) if(both||slot==audible)
        {
            auto p=settings[static_cast<size_t>(slot)].parameters; p.bypass=p.bypass||hostBypass;
            float* pointers[]{work[slot][0].data(),work[slot][1].data()};
            clipped[static_cast<size_t>(slot)]=engines[static_cast<size_t>(slot)].process(pointers,channelCount,n,p,
                settings[static_cast<size_t>(slot)].quality).clippingActivity;
        }
        for(int i=0;i<n;++i)
        {
            if(warmup>0&&--warmup==0) blend.set(static_cast<float>(desired));
            if(matchWarmup>0) --matchWarmup;
            const float mix=blend.next();
            double a=0,b=0;
            for(int ch=0;ch<channelCount;++ch)
            { a+=static_cast<double>(work[0][ch][i])*work[0][ch][i]; b+=static_cast<double>(work[1][ch][i])*work[1][ch][i]; }
            auto gains=matcher.next(a/channelCount,b/channelCount,both&&matchWarmup==0,match&&matchWarmup==0);
            if(hostBypass) gains={1,1};
            for(int ch=0;ch<channelCount;++ch)
            {
                // Do not read a parked slot into an endpoint blend (even 0*NaN).
                const float y=mix==0?work[0][ch][i]*gains[0]:(mix==1?work[1][ch][i]*gains[1]
                    :work[0][ch][i]*gains[0]*(1-mix)+work[1][ch][i]*gains[1]*mix);
                channels[ch][offset+i]=y;
                meters.outputPeak=std::max(meters.outputPeak,std::abs(y)); energy+=static_cast<double>(y)*y;
            }
            activity+=clipped[0]*(1-mix)+clipped[1]*mix;
        }
    }
    meters.outputRms=static_cast<float>(std::sqrt(energy/(static_cast<double>(samples)*channelCount)));
    meters.clippingActivity=static_cast<float>(activity/samples);
    return meters;
}
}
