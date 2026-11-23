#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace noclip
{
enum class Curve { tanh, atan, cubic, knee, asymmetric, count };
inline constexpr int curveCount = static_cast<int>(Curve::count);

// All curves have unit slope at the origin and are bounded by +/- threshold.
// Double precision intermediates avoid overflow for finite float audio inputs.
inline float clip(double x, float threshold, float shape, float knee, Curve curve) noexcept
{
    if (!std::isfinite(x)) return 0;
    const double t = std::isfinite(threshold) ? std::clamp(threshold, 0.0001f, 1.0f) : 1.0;
    const double s = std::isfinite(shape) ? std::clamp(shape, 0.0f, 1.0f) : 0.0;
    const double k = std::isfinite(knee) ? std::clamp(knee, 0.0f, 1.0f) : 0.2;
    const double hard = std::clamp(x, -t, t);
    if (s == 1.0) return static_cast<float>(hard);
    const double u = x / t;
    double soft = 0;
    const auto cubic = [](double v)
    {
        v = std::clamp(v, -1.5, 1.5);
        return v - (4.0 / 27.0) * v * v * v;
    };
    switch (curve)
    {
        case Curve::tanh: soft = std::tanh(u); break;
        case Curve::atan:
            soft = (2.0 / std::numbers::pi) * std::atan((std::numbers::pi / 2.0) * u); break;
        case Curve::cubic: soft = cubic(u); break;
        case Curve::asymmetric: soft = u >= 0 ? cubic(u) : 0.65 * cubic(u / 0.65); break;
        case Curve::knee:
        {
            const double a = std::abs(u);
            if (k < 1.0e-6) soft = std::clamp(u, -1.0, 1.0);
            else if (a <= 1.0 - k) soft = u;
            else if (a >= 1.0 + k) soft = std::copysign(1.0, u);
            else
            {
                const double d = a - (1.0 - k);
                soft = std::copysign(1.0 - k + d - d * d / (4.0 * k), u);
            }
            break;
        }
        default: soft = std::clamp(u, -1.0, 1.0); break;
    }
    return static_cast<float>(std::clamp((1.0 - s) * t * soft + s * hard, -t, t));
}
}
