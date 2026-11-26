#include "DSP/Antiderivative.h"
#include "DSP/ModularDSP.h"
#include <complex>
#include <iostream>
#include <memory>

int main()
{
    int failures = 0;
    const auto check = [&](bool ok, const char* name)
    { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    for (int curve=0; curve<noclip::curveCount; ++curve)
        for (float hardness : {0.f,.4f,1.f})
            for (float knee : {0.f,.2f,1.f})
                for (float threshold : {.05f,1.f})
                    for (int i=-200; i<=200; ++i)
                    {
                        const double x=i*.013, epsilon=1.e-6;
                        const auto family=static_cast<noclip::Curve>(curve);
                        const double derivative=(noclip::clipIntegral(x+epsilon,threshold,hardness,knee,family)
                            -noclip::clipIntegral(x-epsilon,threshold,hardness,knee,family))/(2*epsilon);
                        check(std::abs(derivative-noclip::clip(x,threshold,hardness,knee,family))<2.e-5,
                              "analytical primitive differentiates to each clip transfer");
                    }
    auto table=std::make_unique<noclip::CurveTable>(noclip::CurveTable::make(
        {-1,.5f,-.3f,.2f,.4f,-.5f,.8f,.2f,1}));
    for (int i=-200; i<=200; ++i)
    {
        const double x=i*.013, epsilon=1.e-6;
        const double derivative=(noclip::CurveTable::integral(table->a,table->integralA,x+epsilon)
            -noclip::CurveTable::integral(table->a,table->integralA,x-epsilon))/(2*epsilon);
        check(std::abs(derivative-noclip::CurveTable::evaluate(table->a,x))<2.e-5,
              "custom primitive includes piecewise linear interpolation and constant tails");
    }
    noclip::Antiderivative1 adaa;
    const auto transfer=[](double x) { return noclip::clip(x,.2f,1,0,noclip::Curve::knee); };
    const auto integral=[](double x) { return noclip::clipIntegral(x,.2f,1,0,noclip::Curve::knee); };
    for (double x : {0.,1.e-12,1.e-12,1.,-1.,1.e10,1.e10+1,0.})
    {
        const double y=adaa.process(x,transfer,integral);
        check(std::isfinite(y)&&std::abs(y)<=.200001,"ADAA remains bounded near equal inputs and at extreme drive");
    }
    adaa.reset();
    constexpr int count=8192;
    std::complex<double> direct{}, filtered{};
    for (int i=0; i<count*2; ++i)
    {
        const double x=.5*std::sin(2*std::numbers::pi*9000*i/48000);
        const double y=adaa.process(x,transfer,integral);
        if (i>=count)
        {
            const auto basis=std::polar(1.,-2*std::numbers::pi*21000*i/48000);
            direct+=static_cast<double>(transfer(x))*basis; filtered+=y*basis;
        }
    }
    const double ratio=std::abs(filtered)/std::abs(direct);
    std::cout << "First-order ADAA selected 21 kHz alias reduction: " << -20*std::log10(ratio) << " dB\n";
    check(ratio<.8,"ADAA reduces the selected folded harmonic");

    auto engine=std::make_unique<noclip::ModularDSP>();
    noclip::ModularParameters p; p.active=true; p.adaa=true;
    p.stages[0].enabled=true; p.stages[0].custom=true;
    noclip::AdvancedParameters a;
    engine->prepare(48000,p,a);
    std::array<noclip::OneSample,3> dryReference;
    std::array<noclip::OneSample,2> wetDelay;
    std::array<noclip::HalfSample,2> wetReference;
    for (int i=0; i<2048; ++i)
    {
        const double input=.01*std::sin(i*.1);
        double expected=input, expectedWet=input;
        for (auto& f:dryReference) expected=f.process(expected);
        for (auto& f:wetDelay) expectedWet=f.process(expectedWet);
        for (auto& f:wetReference) expectedWet=f.process(expectedWet);
        engine->advance(std::abs(input));
        const auto result=engine->process(0,input);
        check(std::abs(result[0]-expected)<1.e-8,"global dry follows the three ADAA stage delays");
        check(std::abs(result[1]-expectedWet)<2.e-6,"only active nonlinear stages apply ADAA averaging");
    }
    // A fully preserved clean 808 branch must not acquire ADAA low-pass filtering.
    for (bool enabled : {false,true})
    {
        p={}; p.active=true; p.adaa=true; p.bass=enabled;
        a={}; a.sub=1; a.harmonics=0; a.bassDrive=24;
        engine->prepare(48000,p,a);
        for (int i=0; i<4096; ++i)
        {
            const double input=.1*std::sin(i*1.7)+.1*std::sin(i*.01);
            engine->advance(std::abs(input));
            const auto result=engine->process(0,input);
            check(std::abs(result[0]-result[1])<1.e-8,"disabled stages and clean 808 preserve the aligned dry signal");
        }
    }
    // Biased asymmetric custom shaping across zero must integrate continuously.
    auto shaper=std::make_unique<noclip::AdvancedDSP>();
    a.custom=true; a.adaa=true; a.bias=.2f; a.asymmetry=.7f; a.morph=.3f;
    shaper->prepare(48000,a); shaper->setTable(*table,true);
    auto ordinary=std::make_unique<noclip::AdvancedDSP>();
    a.adaa=false; ordinary->prepare(48000,a); ordinary->setTable(*table,true);
    const std::array<float,noclip::curveCount> weights{0,0,0,1,0};
    double old=0;
    for (double input : {-.8,.7,-.2,-.2+1.e-10,.1,2.,-2.})
    {
        shaper->advance(); ordinary->advance();
        const double actual=shaper->process(0,input,1,1,0,.2f,weights,1)[1];
        double expected=0;
        constexpr int intervals=20000;
        for (int i=0;i<intervals;++i)
            expected+=ordinary->process(0,old+(input-old)*(i+.5)/intervals,1,1,0,.2f,weights,1)[1]/intervals;
        check(std::abs(actual-expected)<2.e-5,"custom ADAA agrees with numerical integration across bias and asymmetry");
        old=input;
    }
    return failures ? 1 : 0;
}

