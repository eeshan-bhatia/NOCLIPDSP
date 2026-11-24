#include "PluginProcessor.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include <complex>
#include <cstdlib>
#include <new>

namespace { thread_local bool monitorAllocations=false; thread_local int allocationCount=0; }
void* operator new(std::size_t n) { if(monitorAllocations) ++allocationCount; if(auto* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

int processingTests()
{
    int failures=0;
    const auto check=[&](bool ok,const char* name) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<name<<'\n'; } };
    auto chain=std::make_unique<noclip::ProcessingChain>();
    const auto curve=noclip::CurveTable::make();
    constexpr int fftSize=8192;
    juce::dsp::FFT fft(13);
    std::array<std::vector<float>,4> spectra;
    std::ofstream cpu("oversampling-cpu.csv"); cpu<<"quality,latency_samples,microseconds_per_stereo_256\n";
    int fixedLatency=-1;
    for(int quality=0;quality<4;++quality)
    {
        noclip::Parameters p; p.clipEnabled=true; p.driveDb=12; p.thresholdDb=-12; p.shape=1;
        chain->prepare(48000,2,p,quality,curve);
        if(fixedLatency<0) fixedLatency=chain->latency();
        check(chain->latency()==fixedLatency,"latency invariant across quality settings");
        std::vector<float> left(fftSize+8192),right(left.size());
        for(size_t i=0;i<left.size();++i) left[i]=right[i]=.5f*static_cast<float>(std::sin(2*std::numbers::pi*9000*static_cast<double>(i)/48000));
        float* channels[]{left.data(),right.data()};
        monitorAllocations=true;
        chain->process(channels,2,static_cast<int>(left.size()),p,quality);
        monitorAllocations=false;
        check(left==right,"oversampled stereo remains coherent");
        spectra[quality].resize(fftSize*2);
        std::copy_n(left.begin()+8192,fftSize,spectra[quality].begin());
        fft.performFrequencyOnlyForwardTransform(spectra[quality].data());
        for(int i=0;i<=fftSize/2;++i) spectra[quality][i]*=2.0f/fftSize;
        std::array<float,256> l,r;
        float* block[]{l.data(),r.data()};
        const auto start=std::chrono::steady_clock::now();
        for(int n=0;n<400;++n)
        {
            for(int i=0;i<256;++i) l[i]=r[i]=.5f*std::sin(static_cast<float>(n*256+i)*.137f);
            chain->process(block,2,256,p,quality);
        }
        const auto micros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/400;
        cpu<<(1<<quality)<<','<<chain->latency()<<','<<micros<<'\n';
        // The peak of the *linear* oversampled impulse must agree with host latency.
        p={}; p.clipEnabled=false; chain->prepare(48000,2,p,quality,curve);
        std::vector<float> impulse(1024,0); impulse[0]=.1f;
        float* mono[]{impulse.data()}; chain->process(mono,1,1024,p,quality);
        const int peak=static_cast<int>(std::max_element(impulse.begin(),impulse.end())-impulse.begin());
        check(peak==chain->latency(),"reported latency matches linear impulse peak");
    }
    // 9000 Hz third harmonic folds to 21000 Hz at 1x; above-Nyquist
    // components should be removed by high-rate processing and decimation.
    constexpr int aliasBin=3584;
    std::cout<<"21kHz alias amplitudes: "; for(auto& s:spectra) std::cout<<s[aliasBin]<<' '; std::cout<<'\n';
    check(spectra[1][aliasBin]<spectra[0][aliasBin]*.5f,"2x measurably reduces selected alias");
    check(spectra[3][aliasBin]<spectra[0][aliasBin]*.25f,"8x measurably reduces selected alias");
    std::ofstream spectrum("oversampling-spectrum.csv"); spectrum<<"frequency,1x,2x,4x,8x\n";
    for(int i=0;i<=fftSize/2;++i)
    {
        spectrum<<static_cast<double>(i)*48000/fftSize;
        for(auto& s:spectra) spectrum<<','<<20*std::log10(std::max(1e-9f,s[i]));
        spectrum<<'\n';
    }
    noclip::Parameters p; p.clipEnabled=true; p.driveDb=24; p.advanced.routing=2;
    chain->prepare(48000,2,p,0,curve);
    std::array<float,257> l,r; float* block[]{l.data(),r.data()};
    float previous=0, maxJump=0;
    for(int b=0;b<300;++b)
    {
        for(int i=0;i<257;++i) l[i]=r[i]=.2f*std::sin(static_cast<float>(b*257+i)*.01f);
        p.advanced.routing=(b/31)%3; p.advanced.lowHz=40+static_cast<float>((b*17)%950);
        p.advanced.highHz=1200+static_cast<float>((b*71)%10800);
        monitorAllocations=true;
        chain->process(block,2,257,p,(b/17)%4);
        monitorAllocations=false;
        for(float x:l) { check(std::isfinite(x)&&std::abs(x)<=1.00001f,"quality/routing automation bounded and finite"); maxJump=std::max(maxJump,std::abs(x-previous)); previous=x; }
    }
    check(maxJump<.2f,"quality and routing changes avoid discontinuity spikes");
    NoclipProcessor processor;
    auto layout=processor.getBusesLayout(); layout.inputBuses.set(1,juce::AudioChannelSet::mono()); processor.setBusesLayout(layout);
    processor.setParameter("scEnabled",1); processor.setParameter("abMatch",1); processor.prepareToPlay(48000,257);
    juce::AudioBuffer<float> audio(3,257); juce::MidiBuffer midi;
    for(int b=0;b<80;++b)
    {
        if(b==20) processor.selectSnapshot(1);
        audio.clear(); for(int i=0;i<257;++i) { audio.setSample(0,i,.2f); audio.setSample(1,i,.2f); audio.setSample(2,i,.8f); }
        monitorAllocations=true; processor.processBlock(audio,midi); monitorAllocations=false;
    }
    check(allocationCount==0,"no standard new/new[] during processing, sidechain, matching and switches");
    // Host bypass must be an exact delay, including when quality is not 1x.
    p={}; p.bypass=true; chain->prepare(48000,1,p,3,curve);
    std::vector<float> bypass(1024,0); bypass[0]=.3f; float* mono[]{bypass.data()};
    chain->process(mono,1,1024,p,3);
    for(int i=0;i<1024;++i) check(bypass[i]==(i==chain->latency()?.3f:0),"bypass exact latency-aligned identity");
    std::cout<<"Processing checks failures: "<<failures<<", latency "<<fixedLatency<<" samples\n";
    return failures;
}
