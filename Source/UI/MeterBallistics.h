#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace noclip::ui
{
// Display-only ballistics. Does not change audio or the processor's measurements.
class MeterBallistics
{
public:
    void update(float in, float out, float rms, float activity, double seconds) noexcept
    {
        const double dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0, 1.0) : 0.0;
        const std::array<float, 2> incoming { decibels(in), decibels(out) };
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            if (incoming[i] >= peaks[i]) { peaks[i] = incoming[i]; hold[i] = 0.35; }
            else
            {
                const double releaseTime = std::max(0.0, dt - hold[i]);
                hold[i] = std::max(0.0, hold[i] - dt);
                peaks[i] = std::max(incoming[i], peaks[i] - static_cast<float>(12.0 * releaseTime));
            }
        }
        const double amplitude = std::isfinite(rms) ? std::max(0.0f, rms) : 0.0;
        const double alpha = -std::expm1(-dt / 0.3);
        rmsPower += alpha * (amplitude * amplitude - rmsPower);
        const float fraction = std::isfinite(activity) ? std::clamp(activity, 0.0f, 1.0f) : 0;
        clip += static_cast<float>(alpha) * (fraction - clip);
    }
    std::array<float, 4> values() const noexcept
    {
        return { peaks[0], peaks[1], decibels(static_cast<float>(std::sqrt(std::max(0.0, rmsPower)))), clip * 100 };
    }
private:
    static float decibels(float amplitude) noexcept
    {
        return std::isfinite(amplitude) && amplitude > 0
            ? std::max(-100.0f, 20.0f * std::log10(amplitude)) : -100.0f;
    }
    std::array<float, 2> peaks { -100, -100 };
    std::array<double, 2> hold {};
    double rmsPower = 0;
    float clip = 0;
};
}
