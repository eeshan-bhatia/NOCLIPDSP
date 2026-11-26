#pragma once
#include "AdvancedDSP.h"
namespace noclip
{
struct ShaperParameters
{
    bool enabled = false, clip = true, custom = false;
    float drive = 0, threshold = 0, hardness = 0, knee = .2f;
    int curve = 3;
    float bias = 0, asymmetry = 0, morph = 0, mix = 1;
};
struct ModularParameters
{
    bool active = false, bass = false, adaa = false;
    float lowHz = 120, highHz = 2500;
    std::array<ShaperParameters, 4> stages;
};
// Reuses the analytical clipper, bounded LUT shaper and phase-matched LR split.
class ModularDSP
{
    struct Stage
    {
        AdvancedDSP shaper;
        std::array<Smooth, 8> controls;
        std::array<Smooth, curveCount> weights;
        std::array<float, 8> v{};
        std::array<float, curveCount> w{};
        float activity = 0;
        bool antialiased = false;
        std::array<Antiderivative1, 2> clipAdaa;
        std::array<HalfSample, 2> clipLinear, shapeLinear;
        std::array<OneSample, 2> dryLinear;
        void set(ShaperParameters p) noexcept
        {
            controls[0].set(p.enabled ? 1.f : 0.f);
            controls[1].set(p.clip ? 1.f : 0.f);
            controls[2].set(p.custom ? 1.f : 0.f);
            controls[3].set(std::pow(10.f, std::clamp(p.drive, 0.f, 36.f) / 20));
            controls[4].set(std::pow(10.f, std::clamp(p.threshold, -36.f, 0.f) / 20));
            controls[5].set(std::clamp(p.hardness, 0.f, 1.f));
            controls[6].set(std::clamp(p.knee, 0.f, 1.f));
            controls[7].set(std::clamp(p.mix, 0.f, 1.f));
            for (int i = 0; i < curveCount; ++i)
                weights[i].set(i == p.curve ? 1.f : 0.f);
            AdvancedParameters a;
            a.custom = true;
            a.bias = p.bias;
            a.asymmetry = p.asymmetry;
            a.morph = p.morph;
            shaper.setParameters(a);
        }
        void prepare(double rate, ShaperParameters p, bool useAdaa)
        {
            AdvancedParameters a;
            a.custom = true;
            a.bias = p.bias;
            a.asymmetry = p.asymmetry;
            a.morph = p.morph;
            antialiased = useAdaa;
            a.adaa = useAdaa;
            for (auto& f : clipAdaa) f.reset();
            for (auto& f : clipLinear) f.reset();
            for (auto& f : shapeLinear) f.reset();
            for (auto& f : dryLinear) f.reset();
            shaper.prepare(rate, a);
            for (auto &c : controls)
                c.prepare(rate, 0);
            for (auto &c : weights)
                c.prepare(rate, 0);
            set(p);
            for (auto &c : controls)
                c.prepare(rate, c.target);
            for (auto &c : weights)
                c.prepare(rate, c.target);
        }
        void advance() noexcept
        {
            for (size_t i = 0; i < v.size(); ++i)
                v[i] = controls[i].next();
            for (size_t i = 0; i < w.size(); ++i)
                w[i] = weights[i].next();
            shaper.advance();
        }
        double process(int ch, double x) noexcept
        {
            activity = 0;
            if (antialiased)
            {
                const auto channel = static_cast<size_t>(ch);
                const double driven = x * v[3];
                const auto transfer = [&](double value)
                {
                    double result = 0;
                    for (int i=0; i<curveCount; ++i) if (w[i]>0)
                        result += w[i] * noclip::clip(value,v[4],v[5],v[6],static_cast<Curve>(i));
                    return result;
                };
                const auto integral = [&](double value)
                {
                    double result = 0;
                    for (int i=0; i<curveCount; ++i) if (w[i]>0)
                        result += w[i] * clipIntegral(value,v[4],v[5],v[6],static_cast<Curve>(i));
                    return result;
                };
                const double clipped = clipAdaa[channel].process(driven,transfer,integral);
                double y = clipLinear[channel].process(driven)*(1-v[1]) + clipped*v[1];
                const auto shaped = shaper.process(ch,y,1,v[4],0,.2f,w,1);
                y = shapeLinear[channel].process(y)*(1-v[2]) + shaped[1]*v[2];
                const double dry = dryLinear[channel].process(x);
                const float active = v[0]*v[7]*std::max(v[1],v[2]);
                activity = std::abs(driven)>v[4] ? active : 0;
                return dry*(1-active) + y*active;
            }
            if (v[0] == 0)
                return x;
            const double driven = x * v[3];
            double clipped = 0;
            for (int i = 0; i < curveCount; ++i)
                if (w[i] > 0)
                    clipped += w[i] * noclip::clip(driven, v[4], v[5], v[6], static_cast<Curve>(i));
            double y = driven * (1 - v[1]) + clipped * v[1];
            if (v[2] > 0)
            {
                const auto shaped = shaper.process(ch, y, 1, v[4], 0, .2f, w, 1);
                y = y * (1 - v[2]) + shaped[1] * v[2];
            }
            const float active = v[0] * v[7] * std::max(v[1], v[2]);
            activity = std::abs(driven) > v[4] ? active : 0;
            return x * (1 - active) + y * active;
        }
    };
    std::array<Stage, 4> stages;
    std::array<ThreeBandCrossover, 2> split;
    std::array<LinkwitzRiley, 2> drySplit;
    std::array<double, 2> dcX{}, dcY{};
    double dcPole = 0;
    AdvancedDSP bass;
    bool antialiased = false;
    std::array<OneSample,2> bandBypass, bandDry, masterDry, bassBypass, bassDry;
    Smooth bandsOn, bassOn, low, high, bassSplit, dc;
    double rate = 48000;
    std::array<float, curveCount> bassWeights{0, 0, 0, 1, 0};

  public:
    void prepare(double sampleRate, const ModularParameters &p, AdvancedParameters a)
    {
        rate = sampleRate;
        antialiased = p.adaa;
        for (auto* filters : {&bandBypass,&bandDry,&masterDry,&bassBypass,&bassDry})
            for (auto& f : *filters) f.reset();
        for (size_t i = 0; i < 4; ++i)
            stages[i].prepare(rate, p.stages[i], antialiased);
        for (auto &c : split)
            c.reset();
        for (auto &c : drySplit)
            c.reset();
        dc.prepare(rate, a.dc ? 1.f : 0.f);
        dcPole = std::exp(-2 * std::numbers::pi * 10 / rate);
        dcX = {};
        dcY = {};
        auto bassParameters = a;
        bassParameters.routing = 2;
        bassParameters.custom = false;
        bassParameters.bias = 0;
        bassParameters.asymmetry = 0;
        bassParameters.dc = false;
        bassParameters.adaa = antialiased;
        bass.prepare(rate, bassParameters);
        bandsOn.prepare(rate, 0);
        bassOn.prepare(rate, 0);
        low.prepare(rate, 0);
        high.prepare(rate, 0);
        bassSplit.prepare(rate, 0);
        set(p, a);
        for (auto *c : {&bandsOn, &bassOn, &low, &high, &bassSplit})
            c->prepare(rate, c->target);
    }
    void set(const ModularParameters &p, AdvancedParameters a) noexcept
    {
        for (size_t i = 0; i < 4; ++i)
            stages[i].set(p.stages[i]);
        bandsOn.set(p.stages[1].enabled || p.stages[2].enabled || p.stages[3].enabled ? 1.f : 0.f);
        bassOn.set(p.bass ? 1.f : 0.f);
        low.set(static_cast<float>(LinkwitzRiley::coefficient(std::clamp(p.lowHz, 40.f, 1000.f), rate)));
        high.set(static_cast<float>(LinkwitzRiley::coefficient(std::clamp(p.highHz, 1200.f, 12000.f), rate)));
        dc.set(a.dc ? 1.f : 0.f);
        bassSplit.set(static_cast<float>(LinkwitzRiley::coefficient(a.splitHz, rate)));
        a.routing = 2;
        a.custom = false;
        a.bias = 0;
        a.asymmetry = 0;
        a.dc = false;
        bass.setParameters(a);
    }
    void setTable(int stage, const CurveTable &table, bool immediate = false) noexcept
    {
        stages[static_cast<size_t>(stage)].shaper.setTable(table, immediate);
    }
    void advance(double amplitude) noexcept
    {
        for (auto &s : stages)
            s.advance();
        bandsOn.next();
        bassOn.next();
        low.next();
        high.next();
        bassSplit.next();
        dc.next();
        bass.advance(amplitude);
    }
    std::array<double, 3> process(int ch, double x) noexcept
    {
        // Keep crossover history current even while all band processors are off.
        const auto parts = split[static_cast<size_t>(ch)].process(x, low.current, high.current);
        const double aligned = parts[0] + parts[1] + parts[2];
        double y = 0, activity = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            y += stages[i + 1].process(ch, parts[i]);
            activity = std::max(activity, static_cast<double>(stages[i + 1].activity));
        }
        const auto channel = static_cast<size_t>(ch);
        y = (antialiased ? bandBypass[channel].process(x) : x) * (1 - bandsOn.current) + y * bandsOn.current;
        double dry = x * (1 - bandsOn.current) + aligned * bandsOn.current;
        if (antialiased) dry = bandDry[channel].process(dry);
        y = stages[0].process(ch, y);
        activity = std::max(activity, static_cast<double>(stages[0].activity));
        if (antialiased) dry = masterDry[channel].process(dry);
        const auto b = bass.process(ch, y, 1, 1, 0, .2f, bassWeights, 1);
        // Bass clean allpass must also be applied to the global dry reference.
        const auto reference = drySplit[static_cast<size_t>(ch)].process(dry, bassSplit.current);
        dry = dry * (1 - bassOn.current) + (reference[0] + reference[1]) * bassOn.current;
        if (antialiased)
        {
            dry = bassDry[channel].process(dry);
            y = bassBypass[channel].process(y)*(1-bassOn.current) + b[1]*bassOn.current;
        }
        else y = y * (1 - bassOn.current) + b[1] * bassOn.current;
        const auto c = static_cast<size_t>(ch);
        const double filtered = (1 + dcPole) * .5 * (y - dcX[c]) + dcPole * dcY[c];
        dcX[c] = y;
        dcY[c] = filtered;
        y = y * (1 - dc.current) + filtered * dc.current;
        return {dry, y, std::max(activity, b[2] * bassOn.current)};
    }
};
} // namespace noclip
