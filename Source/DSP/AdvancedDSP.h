#pragma once
#include "Clipper.h"
#include "CurveTable.h"
#include "Crossover.h"

namespace noclip
{
struct BandParameters { float drive = 0, shape = 0, gain = 0, mix = 1; bool enabled = true; };
struct AdvancedParameters
{
    bool custom = false, dc = false;
    int routing = 0; // Full band, three band, 808.
    float bias = 0, asymmetry = 0, morph = 0;
    float lowHz = 120, highHz = 2500;
    std::array<BandParameters, 3> bands;
    float sub = 1, harmonics = .5f, bassDrive = 0, punch = 0, splitHz = 120, bassOutput = 0;
};

// Small independent smoother; coefficients and all controls advance once/frame.
struct Smooth
{
    float current = 0, target = 0, step = 0;
    int left = 0, length = 960;
    void prepare(double rate, float v) noexcept { length = std::max(1, static_cast<int>(rate*.02)); current=target=v; left=0; }
    void set(float v) noexcept { if(v != target) { target=v; left=length; step=(v-current)/static_cast<float>(length); } }
    float next() noexcept { if(left>0) { current+=step; if(--left==0) current=target; } return current; }
};
class AdvancedDSP
{
public:
    void prepare(double, AdvancedParameters);
    void setParameters(AdvancedParameters) noexcept;
    void setTable(const CurveTable&, bool immediate = false) noexcept;
    void advance(double linkedAmplitude = 0) noexcept;
    // Returns phase-matched dry, wet, and threshold activity before mix/output.
    std::array<double, 3> process(int channel, double dry, float drive, float threshold,
        float shape, float knee, const std::array<float, curveCount>& weights, float weightSum) noexcept;
private:
    enum Control { custom, dc, bias, asym, morph, full, multi, bass, lowG, highG,
                   sub, harmonics, bassDrive, punch, splitG, bassOutput, count };
    std::array<Smooth, count> controls;
    std::array<float, count> v {};
    std::array<std::array<Smooth, 5>, 3> bands;
    std::array<std::array<float, 5>, 3> bandValues {};
    std::array<ThreeBandCrossover, 2> crossovers;
    std::array<LinkwitzRiley, 2> bassClean, bassDirty;
    std::array<double, 2> dcX {}, dcY {}, fast {}, slow {};
    CurveTable table = CurveTable::make(), previous = table;
    Smooth tableBlend;
    double sampleRate=48000, dcPole=0, fastPole=0, slowPole=0;
    double transfer(double x, float t, float s, float k,
                    const std::array<float, curveCount>& weights, float sum) const noexcept;
};
}
