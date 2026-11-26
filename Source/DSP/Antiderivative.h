#pragma once
#include "Clipper.h"

namespace noclip
{
// First-order ADAA: average the transfer over the interval between samples.
// Evaluate BOTH primitives with the current controls to avoid modulation spikes.
struct Antiderivative1
{
    double previous = 0;
    void reset() noexcept { previous = 0; }
    template <typename Transfer, typename Integral>
    double process(double x, Transfer transfer, Integral integral) noexcept
    {
        const double old = previous;
        previous = x;
        const double delta = x - old;
        if (std::abs(delta) <= 1.e-6 * std::max({1.0, std::abs(x), std::abs(old)}))
            return transfer((x + old) * .5);
        return (integral(x) - integral(old)) / delta;
    }
};

struct HalfSample
{
    double previous = 0;
    void reset() noexcept { previous = 0; }
    double process(double x) noexcept
    {
        const double y = .5 * (x + previous);
        previous = x;
        return y;
    }
};
struct OneSample
{
    double previous = 0;
    void reset() noexcept { previous = 0; }
    double process(double x) noexcept
    {
        const double y = previous;
        previous = x;
        return y;
    }
};

inline double clipIntegral(double x, float threshold, float shape, float knee, Curve curve) noexcept
{
    const double t = std::clamp(threshold, .0001f, 1.f), u = x / t, a = std::abs(u);
    const double s = std::clamp(shape, 0.f, 1.f), k = std::clamp(knee, 0.f, 1.f);
    const double hard = a <= 1 ? .5 * a * a : a - .5;
    if (s == 1) return t * t * hard;
    const auto cubic = [](double z)
    {
        z = std::abs(z);
        if (z >= 1.5) return z - .5625;
        return .5 * z * z - z * z * z * z / 27;
    };
    double soft = 0;
    switch (curve)
    {
        case Curve::tanh: soft = a + std::log1p(std::exp(-2 * a)) - std::log(2.0); break;
        case Curve::atan:
        {
            const double c = std::numbers::pi / 2;
            soft = (u * std::atan(c * u) - std::log1p(c * c * u * u) / (2 * c)) / c;
            break;
        }
        case Curve::cubic: soft = cubic(u); break;
        case Curve::asymmetric: soft = u >= 0 ? cubic(u) : .65 * .65 * cubic(u / .65); break;
        case Curve::knee:
        {
            if (k < 1.e-6) { soft = hard; break; }
            const double start = 1 - k, end = 1 + k;
            if (a <= start) soft = .5 * a * a;
            else
            {
                const double d = std::min(a, end) - start;
                soft = .5 * start * start + start * d + .5 * d * d - d * d * d / (12 * k);
                soft += std::max(0.0, a - end);
            }
            break;
        }
        default: soft = hard; break;
    }
    return t * t * ((1 - s) * soft + s * hard);
}
}
