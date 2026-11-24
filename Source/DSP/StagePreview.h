#pragma once
#include "ModularDSP.h"
namespace noclip
{
// Settled local transfer preview, deliberately independent of section ON and
// global input/output, crossovers, DC filtering, oversampling and ceiling.
inline double previewStage(double input, const ShaperParameters &p, const CurveTable &table) noexcept
{
    if (!p.clip && !p.custom)
        return input;
    const double threshold = std::pow(10.f, std::clamp(p.threshold, -36.f, 0.f) / 20);
    double y = input * std::pow(10.f, std::clamp(p.drive, 0.f, 36.f) / 20);
    if (p.clip)
        y = clip(y, static_cast<float>(threshold), p.hardness, p.knee, static_cast<Curve>(p.curve));
    if (p.custom)
    {
        y += p.bias * threshold;
        y *= y >= 0 ? 1 + .75 * p.asymmetry : 1 - .75 * p.asymmetry;
        y = threshold * ((1 - p.morph) * CurveTable::evaluate(table.a, y / threshold) +
                         p.morph * CurveTable::evaluate(table.b, y / threshold));
    }
    return input * (1 - p.mix) + y * p.mix;
}
} // namespace noclip
