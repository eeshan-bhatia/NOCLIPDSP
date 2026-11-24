#include "ProcessingChain.h"

namespace noclip
{
void ProcessingChain::prepare(double rate,int channels,Parameters p,int quality,const CurveTable& curve)
{
    if(!std::isfinite(rate)) rate=48000;
    sampleRate=std::clamp(rate,8000.0,192000.0); channels=std::clamp(channels,1,2);
    detector.prepare(sampleRate); detector.setParameters(p.sidechain);
    latest=curve; latencySamples=0;
    for(size_t q=0;q<banks.size();++q)
    {
        auto& b=banks[q];
        b.oversampler=std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),q,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,true);
        b.oversampler->initProcessing(chunkSize);
        latencySamples=std::max(latencySamples,static_cast<int>(std::lround(b.oversampler->getLatencyInSamples())));
        auto active=p; active.bypass=false;
        b.engine.prepare(sampleRate*static_cast<double>(1u<<q),active);
        b.engine.setTable(curve,true); b.padding.reset();
    }
    jassert(latencySamples<delaySize);
    for(auto& b:banks) b.pad=latencySamples-static_cast<int>(std::lround(b.oversampler->getLatencyInSamples()));
    dryDelay.reset(); current=std::clamp(quality,0,3); next=-1; warmup=fade=0;
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
float ProcessingChain::render(Bank& b,int channels,int samples,Parameters p) noexcept
{
    float* pointers[] { b.work[0].data(),b.work[1].data() };
    for(int ch=0;ch<channels;++ch) std::copy_n(input[ch].data(),samples,pointers[ch]);
    juce::dsp::AudioBlock<float> block(pointers,static_cast<size_t>(channels),static_cast<size_t>(samples));
    auto up=b.oversampler->processSamplesUp(block);
    float* oversampled[] { up.getChannelPointer(0),channels==2?up.getChannelPointer(1):nullptr };
    p.bypass=false; b.engine.setParameters(p);
    const auto meters=b.engine.process(oversampled,channels,static_cast<int>(up.getNumSamples()),relief.data(),
        static_cast<int>(b.oversampler->getOversamplingFactor()));
    b.oversampler->processSamplesDown(block);
    for(int i=0;i<samples;++i) b.padding.frame(b.work[0][i],b.work[1][i],b.pad);
    return meters.clippingActivity;
}
MeterFrame ProcessingChain::process(float* const* channels,int channelCount,int samples,Parameters p,int quality,
                                    const float* const* sidechain,int sideChannels) noexcept
{
    MeterFrame meters;
    if(samples<=0||channelCount<1||channelCount>2||!banks[0].oversampler) return meters;
    quality=std::clamp(quality,0,3);
    if(next<0&&quality!=current)
    {
        next=quality; auto& b=banks[static_cast<size_t>(next)];
        b.oversampler->reset(); b.padding.reset();
        auto active=p; active.bypass=false;
        b.engine.prepare(sampleRate*static_cast<double>(1u<<next),active);
        b.engine.setTable(latest,true);
        warmup=std::max(latencySamples*2,static_cast<int>(sampleRate*.1)); fade=0;
    }
    bypassBlend.set(p.bypass?0.0f:1.0f);
    finalCeiling.set(dbToGain(p.ceilingDb)); finalEnabled.set(p.clipEnabled?1.0f:0.0f);
    detector.setParameters(p.sidechain);
    double energy=0, activity=0;
    for(int offset=0;offset<samples;offset+=chunkSize)
    {
        const int n=std::min(chunkSize,samples-offset);
        for(int i=0;i<n;++i)
        {
            float peak=0;
            if(sidechain) for(int ch=0;ch<std::clamp(sideChannels,0,2);++ch)
                if(std::isfinite(sidechain[ch][offset+i])) peak=std::max(peak,std::abs(sidechain[ch][offset+i]));
            relief[i]=detector.process(peak,sidechain!=nullptr&&sideChannels>0);
        }
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
