#include "AudioEngine.h"
#include <algorithm>
#include <cmath>

namespace noclip
{
namespace {
float bounded(float x, float lo, float hi, float fallback) noexcept
{
    return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback;
}
Parameters sanitise(Parameters p) noexcept
{
    p.inputDb = bounded(p.inputDb, -48, 24, 0);
    p.outputDb = bounded(p.outputDb, -48, 24, 0);
    p.driveDb = bounded(p.driveDb, 0, 36, 0);
    p.thresholdDb = bounded(p.thresholdDb, -36, 0, 0);
    p.ceilingDb = bounded(p.ceilingDb, -24, 0, 0);
    p.shape = bounded(p.shape, 0, 1, 0);
    p.knee = bounded(p.knee, 0, 1, 0.2f);
    p.mix = bounded(p.mix, 0, 1, 1);
    p.curve = std::clamp(p.curve, 0, curveCount - 1);
    return p;
}
}
float dbToGain(float db) noexcept
{
    if (!std::isfinite(db)) return 1.0f;
    return std::pow(10.0f, std::clamp(db, -120.0f, 48.0f) / 20.0f);
}
void Ramp::prepare(double sampleRate) noexcept
{
    if (!std::isfinite(sampleRate)) sampleRate = 48000.0;
    length = std::max(1, static_cast<int>(std::clamp(sampleRate, 1000.0, 1536000.0) * 0.02));
}
void Ramp::reset(float value) noexcept
{
    current = target = value; remaining = 0; step = 0;
}
void Ramp::setTarget(float value) noexcept
{
    if (value == target) return;
    target = value; remaining = length;
    step = (target - current) / static_cast<float>(length);
}
float Ramp::next() noexcept
{
    if (remaining > 0)
    {
        current += step;
        if (--remaining == 0) current = target;
    }
    return current;
}
void AudioEngine::prepare(double sampleRate, Parameters p) noexcept
{
    p = sanitise(p);
    advanced.prepare(sampleRate, p.advanced);
    for (auto* r : { &input, &output, &active, &enabled, &drive, &threshold, &shape, &knee, &ceiling, &mix })
        r->prepare(sampleRate);
    for (auto& r : curves) r.prepare(sampleRate);
    input.reset(dbToGain(p.inputDb)); output.reset(dbToGain(p.outputDb));
    active.reset(p.bypass ? 0.0f : 1.0f);
    enabled.reset(p.clipEnabled ? 1.0f : 0.0f);
    drive.reset(dbToGain(p.driveDb)); threshold.reset(dbToGain(p.thresholdDb));
    shape.reset(p.shape); knee.reset(p.knee);
    ceiling.reset(dbToGain(p.ceilingDb)); mix.reset(p.mix);
    for (int i = 0; i < curveCount; ++i) curves[i].reset(i == p.curve ? 1.0f : 0.0f);
}
void AudioEngine::setParameters(Parameters p) noexcept
{
    p = sanitise(p);
    advanced.setParameters(p.advanced);
    input.setTarget(dbToGain(p.inputDb)); output.setTarget(dbToGain(p.outputDb));
    active.setTarget(p.bypass ? 0.0f : 1.0f);
    enabled.setTarget(p.clipEnabled ? 1.0f : 0.0f);
    drive.setTarget(dbToGain(p.driveDb)); threshold.setTarget(dbToGain(p.thresholdDb));
    shape.setTarget(p.shape); knee.setTarget(p.knee);
    ceiling.setTarget(dbToGain(p.ceilingDb)); mix.setTarget(p.mix);
    for (int i = 0; i < curveCount; ++i) curves[i].setTarget(i == p.curve ? 1.0f : 0.0f);
}
MeterFrame AudioEngine::process(float* const* channels, int channelCount, int samples) noexcept
{
    MeterFrame meter;
    double energy = 0, clipped = 0;
    if (samples <= 0 || channelCount <= 0 || channelCount > 2) return meter;
    for (int i = 0; i < samples; ++i)
    {
        const float inGain = input.next(), outGain = output.next(), blend = active.next();
        double linkedAmplitude=0;
        for(int ch=0;ch<channelCount;++ch)
            if(std::isfinite(channels[ch][i])) linkedAmplitude=std::max(linkedAmplitude,std::abs(static_cast<double>(channels[ch][i])*inGain));
        advanced.advance(linkedAmplitude);
        const float clipBlend = enabled.next(), d = drive.next(), t = threshold.next();
        const float s = shape.next(), k = knee.next(), c = ceiling.next(), m = mix.next();
        std::array<float, curveCount> weights {};
        float weightSum = 0;
        for (int j = 0; j < curveCount; ++j)
        {
            weights[j] = std::max(0.0f, curves[j].next());
            weightSum += weights[j];
        }
        for (int ch = 0; ch < channelCount; ++ch)
        {
            const float raw = channels[ch][i];
            const float x = std::isfinite(raw) ? raw : 0.0f;
            const double dry = static_cast<double>(x) * inGain;
            const double linear = dry * outGain;
            double wet = linear;
            if (clipBlend > 0 && blend > 0)
            {
                const auto paths = advanced.process(ch, dry, d, t, s, k, weights, weightSum);
                const double mixed = (m == 1 ? paths[1] : (m == 0 ? paths[0] : paths[0] * (1 - m) + m * paths[1])) * outGain;
                const double limited = std::clamp(mixed, -static_cast<double>(c), static_cast<double>(c));
                // Activity measures frames above threshold or ceiling, not gain reduction.
                clipped += (std::abs(mixed)>c?1.0:paths[2]) * clipBlend * blend;
                wet = clipBlend == 1 ? limited : linear * (1 - clipBlend) + clipBlend * limited;
            }
            const double mixed = blend == 1 ? wet : (blend == 0 ? static_cast<double>(x) : x * (1 - blend) + blend * wet);
            const float y = std::isfinite(mixed) && std::abs(mixed) < 3.4e38
                ? static_cast<float>(mixed) : 0.0f;
            channels[ch][i] = y;
            meter.inputPeak = std::max(meter.inputPeak, std::abs(x));
            meter.outputPeak = std::max(meter.outputPeak, std::abs(y));
            energy += static_cast<double>(y) * y;
        }
    }
    const double count = static_cast<double>(samples) * channelCount;
    meter.outputRms = static_cast<float>(std::sqrt(energy / count));
    meter.clippingActivity = static_cast<float>(clipped / count);
    return meter;
}
}
