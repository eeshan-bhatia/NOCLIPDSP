#include "DSP/AudioEngine.h"
#include <complex>
#include <iostream>
#include <thread>
#include <vector>
#include <fstream>

namespace {
int failures=0;
void check(bool result,const char* name) { if(!result) { ++failures; std::cerr<<"FAIL: "<<name<<'\n'; } }
double amplitude(const std::vector<float>& data,double hz,double rate)
{
    std::complex<double> sum{};
    for(size_t i=0;i<data.size();++i) sum+=static_cast<double>(data[i])*std::polar(1.0,-2*std::numbers::pi*hz*static_cast<double>(i)/rate);
    return std::abs(sum)*2/static_cast<double>(data.size());
}
std::vector<float> render(noclip::Parameters p,double hz,float amplitudeIn=.3f)
{
    noclip::AudioEngine engine; engine.prepare(48000,p);
    std::vector<float> audio(96000);
    for(size_t i=0;i<audio.size();++i) audio[i]=amplitudeIn*static_cast<float>(std::sin(2*std::numbers::pi*hz*static_cast<double>(i)/48000));
    float* ptr[]{audio.data()}; engine.process(ptr,1,static_cast<int>(audio.size()));
    return {audio.begin()+48000,audio.end()};
}
}
int main()
{
    auto table=noclip::CurveTable::make();
    for(int i=-1000;i<=1000;++i) check(std::abs(noclip::CurveTable::evaluate(table.a,i/1000.0)-i/1000.0)<1e-6,"linear LUT exactness");
    noclip::CurvePoints folded{-1,.8f,-.7f,.1f,0,.9f,-.9f,.6f,1};
    const auto bounded=noclip::CurveTable::build(folded);
    for(size_t i=0;i<bounded.size();++i)
    {
        const auto segment=std::min(size_t(7),i/256);
        check(bounded[i]>=std::min(folded[segment],folded[segment+1])-1e-6f&&bounded[i]<=std::max(folded[segment],folded[segment+1])+1e-6f,"curve interpolation stays inside segment bounds");
    }
    noclip::CurveMailbox mailbox;
    std::atomic<bool> done{false};
    std::thread producer([&] { for(int i=1;i<=3000;++i) { auto value=table; value.a.fill(static_cast<float>(i)); value.b.fill(-static_cast<float>(i)); mailbox.publish(value); } done.store(true); });
    noclip::CurveTable received;
    while(!done.load()) if(mailbox.consume(received))
        for(size_t i=0;i<received.a.size();++i) check(received.a[i]==received.a[0]&&received.b[i]==-received.a[0],"curve publication never tears");
    producer.join();
    std::ofstream response("crossover-response.csv"); response<<"sample_rate,frequency,magnitude_db,phase_radians\n";
    for(double rate:{44100.0,48000.0,96000.0,192000.0}) for(double hz:{30.0,60.0,120.0,300.0,1000.0,2500.0,6000.0,12000.0})
    {
        noclip::ThreeBandCrossover crossover;
        noclip::LinkwitzRiley ap1,ap2;
        const double g1=noclip::LinkwitzRiley::coefficient(120,rate),g2=noclip::LinkwitzRiley::coefficient(2500,rate);
        std::complex<double> measured{},reference{};
        double maximumError=0;
        const int length=static_cast<int>(rate*2);
        for(int i=0;i<length;++i)
        {
            const double x=std::sin(2*std::numbers::pi*hz*i/rate);
            const auto parts=crossover.process(x,g1,g2);
            const auto a=ap1.process(x,g1); const auto b=ap2.process(a[0]+a[1],g2);
            const double y=parts[0]+parts[1]+parts[2]; maximumError=std::max(maximumError,std::abs(y-b[0]-b[1]));
            if(i>=length/2) { const auto phasor=std::polar(1.0,-2*std::numbers::pi*hz*i/rate); measured+=y*phasor; reference+=x*phasor; }
        }
        const double gain=std::abs(measured)/std::abs(reference);
        check(std::abs(gain-1)<.0001,"three-band unity magnitude reconstruction");
        check(maximumError<1e-9,"low-band phase compensation matches cascaded allpass");
        response<<rate<<','<<hz<<','<<20*std::log10(gain)<<','<<std::arg(measured/reference)<<'\n';
    }
    noclip::Parameters p; p.clipEnabled=true; p.advanced.routing=1;
    for(auto& band:p.advanced.bands) band.enabled=false;
    auto dry=render(p,60);
    p.advanced.bands[0].enabled=true; p.advanced.bands[0].drive=24;
    auto low=render(p,60);
    check(amplitude(low,180,48000)>amplitude(dry,180,48000)+.01,"low-band drive creates harmonics independently");
    p.advanced.bands[0].enabled=false; p.advanced.bands[2].enabled=true; p.advanced.bands[2].drive=24;
    auto highOnly=render(p,60);
    check(std::abs(amplitude(highOnly,60,48000)-amplitude(dry,60,48000))<.001,"high band does not drive sub band");
    p={}; p.clipEnabled=true; p.advanced.custom=true;
    noclip::AudioEngine custom; custom.prepare(48000,p);
    auto inverted=noclip::identityPoints; for(auto& point:inverted) point=-point;
    custom.setTable(noclip::CurveTable::make(inverted,inverted),true);
    float sample=.3f; float* ptr[]{&sample}; custom.process(ptr,1,1);
    check(std::abs(sample+.3f)<1e-6,"edited curve changes audio");
    p.advanced.morph=.5f; custom.prepare(48000,p);
    custom.setTable(noclip::CurveTable::make(noclip::identityPoints,inverted),true);
    sample=.3f; custom.process(ptr,1,1);
    check(std::abs(sample)<1e-6,"A/B curve morph interpolates the actual audio transfer");
    p.advanced.morph=0;
    p.advanced.dc=true; p.advanced.bias=.3f;
    const auto dc=render(p,100,.1f);
    double mean=0; for(float x:dc) mean+=x;
    check(std::abs(mean/static_cast<double>(dc.size()))<1e-5,"DC rejection after biased waveshaping");
    p={}; p.clipEnabled=true; p.advanced.routing=2; p.advanced.sub=1; p.advanced.harmonics=1; p.advanced.bassDrive=24;
    const auto bass=render(p,50);
    check(amplitude(bass,150,48000)>.02,"808 generates measurable upper harmonics");
    p.advanced.sub=0;
    const auto dirty=render(p,50);
    check(std::abs(amplitude(bass,50,48000)-.3)<std::abs(amplitude(dirty,50,48000)-.3),"sub preserve reduces fundamental change");
    p.advanced.sub=1; p.advanced.harmonics=0;
    const auto clean=render(p,50);
    check(std::abs(amplitude(clean,50,48000)-.3)<.0001,"clean 808 is unity-magnitude allpass");
    std::ofstream bassResponse("808-harmonics.csv"); bassResponse<<"harmonic,frequency,clean,preserved,dirty\n";
    for(int h=1;h<=20;++h) bassResponse<<h<<','<<50*h<<','<<amplitude(clean,50*h,48000)<<','
        <<amplitude(bass,50*h,48000)<<','<<amplitude(dirty,50*h,48000)<<'\n';
    // Synthetic pitched 808 with a decaying envelope; no external/copyrighted sample.
    std::vector<float> hit(48000), punched;
    for(size_t i=0;i<hit.size();++i)
    {
        const double time=static_cast<double>(i)/48000;
        hit[i]=static_cast<float>(.65*std::exp(-time*5)*std::sin(2*std::numbers::pi*(50*time+.8*(1-std::exp(-time*50)))));
    }
    punched=hit;
    p.advanced.harmonics=1; p.advanced.punch=0;
    noclip::AudioEngine regular, transient; regular.prepare(48000,p);
    p.advanced.punch=1; transient.prepare(48000,p);
    float* hitPtr[]{hit.data()}; float* punchPtr[]{punched.data()};
    regular.process(hitPtr,1,48000); transient.process(punchPtr,1,48000);
    double difference=0;
    for(size_t i=0;i<2400;++i) { difference+=std::abs(hit[i]-punched[i]); check(std::isfinite(punched[i])&&std::abs(punched[i])<=1,"punch on synthetic 808 stays finite and bounded"); }
    check(difference>.1,"punch changes attack processing on a decaying 808");
    std::cout<<"Advanced DSP failures: "<<failures<<'\n'; return failures?1:0;
}
