#pragma once
#include <algorithm>
#include <cmath>

namespace noclip
{
struct SidechainParameters
{
    bool enabled=false;
    float thresholdDb=-24, amountDb=6, attackMs=5, releaseMs=150;
};
// Causal, stereo-linked amplitude detector. Its output attenuates drive only;
// it never enters the audible signal path or turns into make-up gain.
class SidechainDetector
{
public:
    void prepare(double rate) noexcept { sampleRate=std::max(1000.0,rate); envelope=0; reduction=0; }
    void setParameters(SidechainParameters p) noexcept
    {
        const auto finite=[](float v,float fallback) { return std::isfinite(v)?v:fallback; };
        enabled=p.enabled;
        threshold=std::clamp(finite(p.thresholdDb,-24),-60.0f,0.0f);
        amount=std::clamp(finite(p.amountDb,6),0.0f,24.0f);
        attack=std::exp(-1/(sampleRate*std::clamp(finite(p.attackMs,5),.1f,100.0f)*.001));
        release=std::exp(-1/(sampleRate*std::clamp(finite(p.releaseMs,150),10.0f,1000.0f)*.001));
        smoothing=std::exp(-1/(sampleRate*.005));
    }
    float process(float level,bool connected) noexcept
    {
        const double x=connected&&std::isfinite(level)?std::min(1.0e6,static_cast<double>(std::abs(level))):0;
        const double pole=x>envelope?attack:release;
        envelope=pole*envelope+(1-pole)*x;
        const double db=20*std::log10(std::max(1e-12,envelope));
        const double target=enabled&&connected?amount*std::clamp((db-threshold)/12.0,0.0,1.0):0;
        reduction=smoothing*reduction+(1-smoothing)*target;
        return static_cast<float>(std::pow(10.0,-reduction/20));
    }
    float reductionDb() const noexcept { return static_cast<float>(reduction); }
private:
    double sampleRate=48000,envelope=0,reduction=0,attack=0,release=0,smoothing=0;
    float threshold=-24,amount=6;
    bool enabled=false;
};
}
