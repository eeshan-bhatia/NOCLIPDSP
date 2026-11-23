#include "DSP/AudioEngine.h"
#include "UI/MeterBallistics.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void check(bool ok, const char* name)
{
    if (!ok) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
}
bool near(float a, float b, float tolerance = 1.0e-5f) { return std::abs(a - b) < tolerance; }
}
int main()
{
    noclip::ui::MeterBallistics displayMeter;
    displayMeter.update(1, 1, 1, 1, 0.01);
    displayMeter.update(0, 0, 0, 0, 0.35);
    check(near(displayMeter.values()[0], 0), "meter holds peak for 350ms");
    displayMeter.update(0, 0, 0, 0, 0.5);
    check(near(displayMeter.values()[0], -6), "meter release is 12dB per second");
    noclip::ui::MeterBallistics slow, fast;
    for (int i = 0; i < 10; ++i) slow.update(0.5f, 0.5f, 0.5f, 0.5f, 0.1);
    for (int i = 0; i < 100; ++i) fast.update(0.5f, 0.5f, 0.5f, 0.5f, 0.01);
    check(near(slow.values()[2], fast.values()[2]), "RMS averaging independent of display rate");
    check(near(slow.values()[3], fast.values()[3], 0.0001f), "clip averaging independent of display rate");
    noclip::AudioEngine outputBoost;
    outputBoost.prepare(48000, { 0, 24, false });
    float boosted = 0.01f;
    float* boostChannel[] { &boosted };
    outputBoost.process(boostChannel, 1, 1);
    check(near(boosted, 0.01f * noclip::dbToGain(24)), "output reaches +24dB with clip disabled");
    check(near(noclip::dbToGain(0), 1), "0 dB");
    check(near(noclip::dbToGain(-6.0206f), 0.5f), "dB conversion");
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    for (int block : { 1, 32, 64, 128, 256, 512, 1024 })
    {
        std::array<float, 1024> left {}, right {};
        for (int i = 0; i < block; ++i) left[i] = right[i] = 0.4f * std::sin(static_cast<float>(i));
        const auto original = left;
        float* channels[] { left.data(), right.data() };
        noclip::AudioEngine engine;
        engine.prepare(rate);
        engine.process(channels, 2, block);
        check(left == original && right == original, "unity stereo exact passthrough");
        engine.prepare(rate, { 6.0206f, 0, false });
        engine.process(channels, 2, block);
        for (int i = 0; i < block; ++i) check(near(left[i], 2 * original[i]), "input gain");
        left = original;
        engine.prepare(rate, { 24, 12, true });
        engine.process(channels, 1, block);
        check(left == original, "bypass ignores gains");
    }
    noclip::AudioEngine engine;
    engine.prepare(48000);
    std::array<float, 2048> samples;
    samples.fill(0.25f);
    float* pointer[] { samples.data() };
    engine.setParameters({ 6.0206f, 0, false });
    engine.process(pointer, 1, 2048);
    check(samples[0] > 0.25f && samples[0] < 0.251f, "gain transition smoothed");
    check(near(samples[959], 0.5f) && near(samples.back(), 0.5f), "ramp reaches target");
    samples.fill(0.5f);
    engine.prepare(48000);
    const auto meter = engine.process(pointer, 1, 2048);
    check(near(meter.outputRms, 0.5f) && near(meter.outputPeak, 0.5f), "meters measured");
    samples[0] = std::numeric_limits<float>::quiet_NaN();
    samples[1] = std::numeric_limits<float>::infinity();
    engine.process(pointer, 1, 2);
    check(samples[0] == 0 && samples[1] == 0, "invalid input sanitised");
        check(engine.process(pointer, 1, 0).outputPeak == 0, "empty block");
    check(near(noclip::dbToGain(std::numeric_limits<float>::quiet_NaN()), 1), "invalid gain sanitised");

    std::array<float, 2048> whole, partitioned;
    whole.fill(0.3f); partitioned = whole;
    noclip::AudioEngine continuous, split;
    continuous.prepare(48000); split.prepare(48000);
    continuous.setParameters({ -12, 3, false });
    split.setParameters({ -12, 3, false });
    float* a[] { whole.data() };
    continuous.process(a, 1, 2048);
    int offset = 0;
    for (int size : { 1, 31, 127, 513, 1024, 352 })
    {
        float* b[] { partitioned.data() + offset };
        split.process(b, 1, size);
        offset += size;
    }
    check(whole == partitioned, "block partition invariance");
    samples.fill(0.25f);
    engine.prepare(48000, { 6.0206f, 0, false });
    engine.setParameters({ 6.0206f, 0, true });
    engine.process(pointer, 1, 2048);
    check(samples[0] < 0.5f && samples[0] > 0.499f, "bypass transition smoothed");
    check(near(samples[959], 0.25f), "bypass reaches original");
    std::cout << (failures == 0 ? "All DSP checks passed\n" : "DSP checks failed\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
