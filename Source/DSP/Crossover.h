#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace noclip
{
// TPT Butterworth SVF. Cascaded low and high branches form a fourth-order
// Linkwitz-Riley pair; their sum is an allpass, not a zero-phase wire.
class LinkwitzRiley
{
    struct Section
    {
        double s1 = 0, s2 = 0;
        std::array<double, 2> process(double x, double g) noexcept
        {
            constexpr double k = 1.4142135623730951;
            const double a = 1 / (1 + g*(g+k));
            const double v1 = a*(s1 + g*(x-s2));
            const double v2 = s2 + g*v1;
            s1 = 2*v1-s1; s2 = 2*v2-s2;
            return { v2, x-k*v1-v2 };
        }
    };
public:
    static double coefficient(double hz, double rate) noexcept
    { return std::tan(std::numbers::pi * std::clamp(hz, 10.0, rate * .45) / rate); }
    std::array<double, 2> process(double x, double g) noexcept
    {
        const auto first = common.process(x, g);
        return { low.process(first[0], g)[0], high.process(first[1], g)[1] };
    }
    void reset() noexcept { common = {}; low = {}; high = {}; }
private:
    Section common, low, high;
};
class ThreeBandCrossover
{
public:
    std::array<double, 3> process(double x, double lowG, double highG) noexcept
    {
        const auto first = lower.process(x, lowG);
        const auto upperBands = upper.process(first[1], highG);
        const auto compensation = lowPhase.process(first[0], highG);
        return { compensation[0] + compensation[1], upperBands[0], upperBands[1] };
    }
    void reset() noexcept { lower.reset(); upper.reset(); lowPhase.reset(); }
private:
    LinkwitzRiley lower, upper, lowPhase;
};
}
