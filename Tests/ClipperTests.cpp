#include "DSP/AudioEngine.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

namespace {
bool track = false;
int allocations = 0, failures = 0;
void check(bool ok, const char* name)
{
    if (!ok) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
}
bool near(float a, float b, float tolerance = 2.0e-5f) { return std::abs(a - b) <= tolerance; }
float run(noclip::Parameters p, float sample)
{
    noclip::AudioEngine engine;
    engine.prepare(48000, p);
    float* ptr[] { &sample };
    engine.process(ptr, 1, 1);
    return sample;
}
}
void* operator new(std::size_t size)
{
    if (track) ++allocations;
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

int main()
{
    using noclip::Curve;
    check(near(noclip::clip(2, 0.5f, 1, 0.2f, Curve::tanh), 0.5f), "hard positive");
    check(near(noclip::clip(-2, 0.5f, 1, 0.2f, Curve::tanh), -0.5f), "hard negative");
    check(near(noclip::clip(0.125, 0.5f, 1, 0.2f, Curve::tanh), 0.125f), "hard linear");
    for (int ci = 0; ci < noclip::curveCount; ++ci)
    {
        const auto curve = static_cast<Curve>(ci);
        for (float t : { 0.0158489f, 0.25f, 1.0f })
        for (float knee : { 0.0f, 0.2f, 1.0f })
        for (float shape : { 0.0f, 0.5f, 1.0f })
        {
            float previous = -t;
            for (int i = -1000; i <= 1000; ++i)
            {
                const float x = static_cast<float>(i) * 0.01f;
                const float y = noclip::clip(x, t, shape, knee, curve);
                check(std::isfinite(y) && std::abs(y) <= t, "finite bounded transfer");
                check(y >= previous - 1.0e-6f, "monotone transfer");
                if (curve != Curve::asymmetric)
                    check(near(y, -noclip::clip(-x, t, shape, knee, curve)), "symmetric transfer");
                previous = y;
            }
            check(noclip::clip(0, t, shape, knee, curve) == 0, "zero origin");
        }
        check(std::isfinite(noclip::clip(std::numeric_limits<float>::max(), 1, 0, 1, curve)),
              "max finite input");
    }
    for (float boundary : { 0.8f, 1.2f, -0.8f, -1.2f })
        check(std::abs(noclip::clip(boundary - 1e-5, 1, 0, 0.2f, Curve::knee)
                    - noclip::clip(boundary + 1e-5, 1, 0, 0.2f, Curve::knee)) < 3e-5,
              "piecewise knee continuity");
    for (float boundary : { -1.5f, 1.5f })
        check(std::abs(noclip::clip(boundary - 1e-5, 1, 0, 0, Curve::cubic)
                    - noclip::clip(boundary + 1e-5, 1, 0, 0, Curve::cubic)) < 3e-5,
              "cubic continuity");
    check(std::abs(noclip::clip(1, 1, 0, 0, Curve::asymmetric)
                 + noclip::clip(-1, 1, 0, 0, Curve::asymmetric)) > 0.1f, "audible asymmetry");
    const float soft = noclip::clip(0.8, 1, 0, 0.2f, Curve::tanh);
    const float hard = noclip::clip(0.8, 1, 1, 0.2f, Curve::tanh);
    check(std::abs(soft - hard) > 0.1f, "soft/hard different");
    check(near(noclip::clip(0.8, 1, 0.5f, 0.2f, Curve::tanh), (soft + hard) * 0.5f), "shape morph");

    noclip::Parameters p;
    p.clipEnabled = true; p.shape = 1; p.driveDb = 12; p.thresholdDb = -6.0206f;
    check(near(run(p, 0.4f), 0.5f), "engine drive and threshold");
    p.outputDb = 12; p.ceilingDb = -12;
    check(near(run(p, 0.4f), noclip::dbToGain(-12)), "ceiling after output");
    check(near(run(p, std::numeric_limits<float>::max()), noclip::dbToGain(-12)), "extreme finite ceiling");
    p.outputDb = 0; p.ceilingDb = 0;
    p.mix = 0;
    check(near(run(p, 0.4f), 0.4f), "dry mix excludes drive");
    p.mix = 0.5f;
    check(near(run(p, 0.4f), 0.45f), "half mix");
    p.clipEnabled = false; p.mix = 1;
    check(near(run(p, 0.4f), 0.4f), "disabled clip excludes drive and ceiling");
    p.clipEnabled = true; p.bypass = true; p.inputDb = 24; p.outputDb = 12;
    check(run(p, 0.4f) == 0.4f, "global bypass ignores clipping");
    p = {}; p.clipEnabled = true;
    check(run(p, 0.4f) == 0.4f, "default knee leaves quiet samples unchanged");

    std::array<float, 4096> left {}, right {};
    float* channels[] { left.data(), right.data() };
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    for (int block : { 1, 32, 64, 128, 256, 512, 1024 })
    {
        noclip::AudioEngine engine;
        p.driveDb = 30; p.thresholdDb = -18; p.ceilingDb = -6;
        engine.prepare(rate, p);
        for (int i = 0; i < block; ++i) left[i] = right[i] = std::sin(static_cast<float>(i) * 0.2f);
        const auto meter = engine.process(channels, 2, block);
        check(meter.clippingActivity >= 0 && meter.clippingActivity <= 1, "activity range");
        for (int i = 0; i < block; ++i)
            check(left[i] == right[i] && std::isfinite(left[i]) && std::abs(left[i]) <= noclip::dbToGain(-6),
                  "stereo and ceiling across sample rates and buffers");
    }
    noclip::AudioEngine engine;
    p = {}; p.clipEnabled = true; p.curve = 0;
    engine.prepare(48000, p);
    p.curve = 4;
    engine.setParameters(p);
    left.fill(-0.9f); right = left;
    track = true;
    engine.process(channels, 2, 4096);
    p.driveDb = 36; p.thresholdDb = -36; p.mix = 0.25f; p.clipEnabled = false;
    engine.setParameters(p);
    track = false;
    check(allocations == 0, "no standard heap allocations in processing or updates");
    for (int i = 1; i < 960; ++i)
        check(std::abs(left[i] - left[i - 1]) < 0.002f, "curve switch smoothed");
    check(near(left[959], noclip::clip(-0.9, 1, 0, 0.2f, Curve::asymmetric)), "curve reaches target");

    // Combined automation gives identical output regardless of buffer partitioning.
    std::array<float, 4096> whole, split;
    for (int i = 0; i < 4096; ++i) whole[i] = std::sin(static_cast<float>(i) * 0.3f);
    split = whole;
    noclip::AudioEngine a, b;
    a.prepare(48000); b.prepare(48000);
    p.clipEnabled = true; p.curve = 2; p.bypass = false;
    a.setParameters(p); b.setParameters(p);
    float* full[] { whole.data() };
    a.process(full, 1, 4096);
    int offset = 0;
    for (int size : { 1, 31, 513, 1024, 2527 })
    {
        float* part[] { split.data() + offset };
        b.process(part, 1, size); offset += size;
    }
    check(whole == split, "clip automation block partition invariance");
    check(noclip::clip(std::numeric_limits<double>::infinity(), 1, 0, 0, Curve::tanh) == 0,
          "nonfinite transfer input");
    std::cout << (failures == 0 ? "All clipper checks passed\n" : "Clipper checks failed\n");
    return failures == 0 ? 0 : 1;
}
