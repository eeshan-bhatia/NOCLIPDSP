#pragma once
#include <array>
#include <cstddef>
#include "Clipper.h"
#include "AdvancedDSP.h"
#include "ModularDSP.h"
#include <memory>

namespace noclip
{
float dbToGain(float db) noexcept;

// Finite-duration linear ramps: advancing once per frame keeps stereo coherent.
class Ramp
{
public:
    void prepare(double sampleRate) noexcept;
    void reset(float value) noexcept;
    void setTarget(float value) noexcept;
    float next() noexcept;
private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0, length = 1;
};

struct Parameters
{
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    bool bypass = false;
    bool clipEnabled = false;
    float driveDb = 0.0f, thresholdDb = 0.0f, shape = 0.0f, knee = 0.2f;
    float ceilingDb = 0.0f, mix = 1.0f;
    int curve = static_cast<int>(Curve::knee);
    AdvancedParameters advanced;
    ModularParameters modular;
};

struct MeterFrame
{
    float inputPeak = 0.0f, outputPeak = 0.0f, outputRms = 0.0f;
    float clippingActivity = 0.0f;
};

// No JUCE dependency, allocation, locks or I/O. Supports arbitrary block lengths.
class AudioEngine
{
public:
    void prepare(double sampleRate, Parameters initial = {}) noexcept;
    void setParameters(Parameters parameters) noexcept;
    void setTable(const CurveTable& table, bool immediate = false) noexcept { advanced.setTable(table, immediate); }
    MeterFrame process(float* const* channels, int channelCount, int samples) noexcept;
    void setStageTable(int stage,const CurveTable& table,bool immediate=false) noexcept { if(modular) modular->setTable(stage,table,immediate); }
private:
    Ramp input, output, active;
    Ramp enabled, drive, threshold, shape, knee, ceiling, mix;
    std::array<Ramp, curveCount> curves;
    AdvancedDSP advanced;
    std::unique_ptr<ModularDSP> modular;
    bool useModular=false, antialiased=false;
    std::array<std::array<double,3>,2> linearDelay {};
    int linearPosition=0;
};
}
