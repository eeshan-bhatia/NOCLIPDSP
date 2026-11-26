#include "ProcessingChain.h"

namespace noclip
{
void ProcessingChain::prepare(double rate,int channels,Parameters p,int quality,const CurveTable& curve)
{
    if(!std::isfinite(rate)) rate=48000;
    sampleRate=std::clamp(rate,8000.0,192000.0); channels=std::clamp(channels,1,2);
    latest=curve; stageTables.fill(curve); latencySamples=0;
    for(size_t q=0;q<banks.size();++q)
    {
        auto& b=banks[q];
        const auto exponent = q % qualityCount;
        b.adaa = q >= qualityCount && p.modular.active;
        b.oversampler=std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),exponent,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,true);
        b.oversampler->initProcessing(chunkSize);
        const int ratio = 1 << exponent;
        b.innerPad = b.adaa ? (ratio - 3 % ratio) % ratio : 0;
        b.intrinsic = static_cast<int>(std::lround(b.oversampler->getLatencyInSamples()))
                      + (b.adaa ? (3+b.innerPad)/ratio : 0);
        latencySamples=std::max(latencySamples,b.intrinsic);
        auto active=p; active.bypass=false; active.modular.adaa=b.adaa;
        b.engine.prepare(sampleRate*static_cast<double>(1u<<exponent),active);
        b.engine.setTable(curve,true); b.padding.reset(); b.oversampledPadding.reset();
        for (int stage=0; stage<4; ++stage) b.engine.setStageTable(stage,curve,true);
    }
    jassert(latencySamples<delaySize);
    for(auto& b:banks) b.pad=latencySamples-b.intrinsic;
    dryDelay.reset(); current=std::clamp(quality,0,qualityCount-1)+(p.modular.active&&p.modular.adaa?qualityCount:0); next=-1; warmup=fade=0;
    fadeLength=std::max(1,static_cast<int>(sampleRate*.02));
    bypassBlend.prepare(sampleRate,p.bypass?0.0f:1.0f);
    finalCeiling.prepare(sampleRate,dbToGain(p.ceilingDb));
    finalEnabled.prepare(sampleRate,p.clipEnabled?1.0f:0.0f);
}
void ProcessingChain::setTable(const CurveTable& table) noexcept
{
    latest=table;
    for(auto& b:banks) b.engine.setTable(table);
}
void ProcessingChain::setStageTable(int stage,const CurveTable& table,bool immediate) noexcept
{ stageTables[static_cast<size_t>(stage)]=table; for(auto& b:banks)b.engine.setStageTable(stage,table,immediate); }
float ProcessingChain::render(Bank& b,int channels,int samples,Parameters p) noexcept
{
    float* pointers[] { b.work[0].data(),b.work[1].data() };
    for(int ch=0;ch<channels;++ch) std::copy_n(input[ch].data(),samples,pointers[ch]);
    juce::dsp::AudioBlock<float> block(pointers,static_cast<size_t>(channels),static_cast<size_t>(samples));
    auto up=b.oversampler->processSamplesUp(block);
    float* oversampled[] { up.getChannelPointer(0),channels==2?up.getChannelPointer(1):nullptr };
    p.bypass=false; p.modular.adaa=b.adaa; b.engine.setParameters(p);
    const auto meters=b.engine.process(oversampled,channels,static_cast<int>(up.getNumSamples()));
    // Complete the ADAA delay to a whole host sample BEFORE downsampling.
    // Host-rate linear interpolation would otherwise add unwanted treble loss.
    if (b.innerPad > 0)
        for (size_t i=0; i<up.getNumSamples(); ++i)
        {
            float right=channels==2 ? oversampled[1][i] : 0;
            b.oversampledPadding.frame(oversampled[0][i],right,b.innerPad);
            if (channels==2) oversampled[1][i]=right;
        }
    b.oversampler->processSamplesDown(block);
    for(int i=0;i<samples;++i) b.padding.frame(b.work[0][i],b.work[1][i],b.pad);
    return meters.clippingActivity;
}
MeterFrame ProcessingChain::process(float* const* channels,int channelCount,int samples,Parameters p,int quality) noexcept
{
    MeterFrame meters;
    if(samples<=0||channelCount<1||channelCount>2||!banks[0].oversampler) return meters;
    quality=std::clamp(quality,0,qualityCount-1)+(p.modular.active&&p.modular.adaa?qualityCount:0);
    if(next<0&&quality!=current)
    {
        next=quality; auto& b=banks[static_cast<size_t>(next)];
        b.oversampler->reset(); b.padding.reset(); b.oversampledPadding.reset();
        auto active=p; active.bypass=false; active.modular.adaa=b.adaa;
        b.engine.prepare(sampleRate*static_cast<double>(1u<<(next%qualityCount)),active);
        b.engine.setTable(latest,true);
        for(int i=0;i<4;++i) b.engine.setStageTable(i,stageTables[static_cast<size_t>(i)],true);
        warmup=std::max(latencySamples*2,static_cast<int>(sampleRate*.1)); fade=0;
    }
    bypassBlend.set(p.bypass?0.0f:1.0f);
    finalCeiling.set(dbToGain(p.ceilingDb)); finalEnabled.set(p.clipEnabled?1.0f:0.0f);
    double energy=0, activity=0;
    for(int offset=0;offset<samples;offset+=chunkSize)
    {
        const int n=std::min(chunkSize,samples-offset);
        for(int ch=0;ch<channelCount;++ch) for(int i=0;i<n;++i)
        {
            const float raw=channels[ch][offset+i];
            input[ch][i]=std::isfinite(raw)?std::clamp(raw,-1.0e12f,1.0e12f):0;
            meters.inputPeak=std::max(meters.inputPeak,std::abs(input[ch][i]));
        }
        const float clipped=render(banks[static_cast<size_t>(current)],channelCount,n,p);
        const int replacement=next;
        if(replacement>=0) render(banks[static_cast<size_t>(replacement)],channelCount,n,p);
        for(int i=0;i<n;++i)
        {
            float l=input[0][i],r=channelCount==2?input[1][i]:0;
            dryDelay.frame(l,r,latencySamples);
            const float active=bypassBlend.next(), ceiling=finalCeiling.next(), enabled=finalEnabled.next();
            float blend=0;
            if(replacement>=0)
            {
                if(warmup>0) --warmup;
                else { fade=std::min(fade+1,fadeLength); blend=static_cast<float>(fade)/static_cast<float>(fadeLength); }
            }
            for(int ch=0;ch<channelCount;++ch)
            {
                float y=banks[static_cast<size_t>(current)].work[ch][i];
                if(replacement>=0) y=y*(1-blend)+banks[static_cast<size_t>(replacement)].work[ch][i]*blend;
                // Reconstruction can overshoot. Final sample guard is not true-peak limiting.
                y=y*(1-enabled)+std::clamp(y,-ceiling,ceiling)*enabled;
                y=(ch==0?l:r)*(1-active)+y*active;
                if(!std::isfinite(y)) y=0;
                channels[ch][offset+i]=y;
                meters.outputPeak=std::max(meters.outputPeak,std::abs(y)); energy+=static_cast<double>(y)*y;
            }
            activity+=clipped*active;
        }
        if(replacement>=0&&fade==fadeLength) { current=replacement; next=-1; }
    }
    meters.outputRms=static_cast<float>(std::sqrt(energy/(static_cast<double>(samples)*channelCount)));
    meters.clippingActivity=static_cast<float>(activity/samples);
    return meters;
}
}
