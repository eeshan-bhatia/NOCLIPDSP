#pragma once
#include "AdvancedDSP.h"
namespace noclip
{
// Analysis only: these filters never write into the audible signal path.
class LowEndMonitor
{
    LinkwitzRiley inputFilter;
    std::array<LinkwitzRiley, 2> outputFilters;
    Smooth inputCoefficient, outputCoefficient;
    double rate = 48000, release = 0, envelope = 0;

  public:
    void prepare(double sampleRate, float cutoff) noexcept
    {
        rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 192000.0) : 48000;
        inputFilter.reset();
        for (auto &f : outputFilters)
            f.reset();
        envelope = 0;
        release = std::exp(-1 / (rate * .35));
        const float g = static_cast<float>(LinkwitzRiley::coefficient(cutoff, rate));
        inputCoefficient.prepare(rate, g);
        outputCoefficient.prepare(rate, g);
    }
    void setCutoff(float hz) noexcept
    {
        const float g = static_cast<float>(
            LinkwitzRiley::coefficient(std::clamp(std::isfinite(hz) ? hz : 120.f, 40.f, 400.f), rate));
        inputCoefficient.set(g);
        outputCoefficient.set(g);
    }
    float input(float x) noexcept
    {
        return static_cast<float>(inputFilter.process(std::isfinite(x) ? x : 0, inputCoefficient.next())[0]);
    }
    float output(float left, float right, int channels) noexcept
    {
        const float g = outputCoefficient.next();
        const double l = outputFilters[0].process(std::isfinite(left) ? left : 0, g)[0];
        const double r = channels > 1 ? outputFilters[1].process(std::isfinite(right) ? right : 0, g)[0] : 0;
        envelope = std::max(std::max(std::abs(l), std::abs(r)), envelope * release);
        return static_cast<float>(l);
    }
    float peak() const noexcept
    {
        return static_cast<float>(envelope);
    }
};
} // namespace noclip
