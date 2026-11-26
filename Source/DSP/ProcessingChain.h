#pragma once
#include "AudioEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace noclip
{
// Every quality has the same reported latency. Only the selected bank runs,
// plus the replacement bank while warming up/crossfading. No callback allocation.
class ProcessingChain
{
public:
    void prepare(double rate, int channels, Parameters, int quality, const CurveTable&);
    MeterFrame process(float* const*, int channels, int samples, Parameters, int quality) noexcept;
    void setStageTable(int stage,const CurveTable& table,bool immediate=false) noexcept;
    void setTable(const CurveTable&) noexcept;
    int latency() const noexcept { return latencySamples; }
    int activeQuality() const noexcept { return current % qualityCount; }
private:
    static constexpr int qualityCount = 5, chunkSize = 256, delaySize = 4096;
    struct Delay
    {
        std::array<std::array<float,delaySize>,2> data {};
        int position=0;
        void reset() noexcept { for(auto& ch:data) ch.fill(0); position=0; }
        void frame(float& l,float& r,int delay) noexcept
        {
            const int read=(position+delaySize-delay)%delaySize;
            data[0][position]=l; data[1][position]=r;
            l=data[0][read]; r=data[1][read]; position=(position+1)%delaySize;
        }
    };
    struct Bank
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        AudioEngine engine;
        Delay padding, oversampledPadding;
        std::array<std::array<float,chunkSize>,2> work {};
        int pad=0, intrinsic=0, innerPad=0;
        bool adaa=false;
    };
    std::array<Bank,qualityCount*2> banks;
    Delay dryDelay;
    std::array<std::array<float,chunkSize>,2> input {};
    CurveTable latest=CurveTable::make();
    std::array<CurveTable,4> stageTables;
    Smooth bypassBlend, finalCeiling, finalEnabled;
    double sampleRate=48000;
    int latencySamples=0, current=0, next=-1, warmup=0, fade=0, fadeLength=960;
    float render(Bank&, int channels, int samples, Parameters) noexcept;
};
}
