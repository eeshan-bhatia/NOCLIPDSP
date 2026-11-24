#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace noclip::parameters
{
inline constexpr auto input = "inputDb";
inline constexpr auto output = "outputDb";
inline constexpr auto bypass = "bypass";
inline constexpr auto clipEnabled = "clipEnabled";
inline constexpr auto curve = "curve";
struct FloatSpec
{
    const char *id;
    const char *name;
    const char *unit;
    float min, max, initial;
};
inline constexpr std::array<FloatSpec, 8> floats{{{input, "Input", "dB", -48, 24, 0},
                                                  {output, "Output", "dB", -48, 24, 0},
                                                  {"driveDb", "Drive", "dB", 0, 36, 0},
                                                  {"thresholdDb", "Threshold", "dB", -36, 0, 0},
                                                  {"shape", "Shape", "", 0, 1, 0},
                                                  {"knee", "Knee", "", 0, 1, 0.2f},
                                                  {"ceilingDb", "Ceiling", "dB", -24, 0, 0},
                                                  {"mix", "Mix", "", 0, 1, 1}}};
inline constexpr std::array<const char *, 11> ids{{input, output, bypass, clipEnabled, curve, "driveDb",
                                                   "thresholdDb", "shape", "knee", "ceilingDb", "mix"}};
inline juce::StringArray curveNames()
{
    return {"Tanh", "Arctangent", "Cubic", "Adjustable knee", "Asymmetric"};
}
inline constexpr std::array<FloatSpec, 27> extraFloats{{{"bias", "Bias", "", -.5f, .5f, 0},
                                                        {"asymmetry", "Asymmetry", "", -1, 1, 0},
                                                        {"curveMorph", "Curve A/B morph", "", 0, 1, 0},
                                                        {"subPreserve", "Sub preserve", "", 0, 1, 1},
                                                        {"harmonics", "Harmonics", "", 0, 1, .5f},
                                                        {"bassDrive", "808 drive", "dB", 0, 36, 0},
                                                        {"punch", "Punch", "", 0, 1, 0},
                                                        {"splitHz", "808 split", "Hz", 40, 400, 120},
                                                        {"bassOutput", "808 output", "dB", -24, 12, 0},
                                                        {"curveA0", "Curve A point 1", "", -1, 1, -1},
                                                        {"curveA1", "Curve A point 2", "", -1, 1, -.75f},
                                                        {"curveA2", "Curve A point 3", "", -1, 1, -.5f},
                                                        {"curveA3", "Curve A point 4", "", -1, 1, -.25f},
                                                        {"curveA4", "Curve A point 5", "", -1, 1, 0},
                                                        {"curveA5", "Curve A point 6", "", -1, 1, .25f},
                                                        {"curveA6", "Curve A point 7", "", -1, 1, .5f},
                                                        {"curveA7", "Curve A point 8", "", -1, 1, .75f},
                                                        {"curveA8", "Curve A point 9", "", -1, 1, 1},
                                                        {"curveB0", "Curve B point 1", "", -1, 1, -1},
                                                        {"curveB1", "Curve B point 2", "", -1, 1, -.75f},
                                                        {"curveB2", "Curve B point 3", "", -1, 1, -.5f},
                                                        {"curveB3", "Curve B point 4", "", -1, 1, -.25f},
                                                        {"curveB4", "Curve B point 5", "", -1, 1, 0},
                                                        {"curveB5", "Curve B point 6", "", -1, 1, .25f},
                                                        {"curveB6", "Curve B point 7", "", -1, 1, .5f},
                                                        {"curveB7", "Curve B point 8", "", -1, 1, .75f},
                                                        {"curveB8", "Curve B point 9", "", -1, 1, 1}}};
// Validation-only metadata for the development schema-3 multiband build.
inline constexpr std::array<FloatSpec, 17> legacyBandFloats{{
    {"lowHz", "Low crossover", "Hz", 40, 1000, 120},
    {"highHz", "High crossover", "Hz", 1200, 12000, 2500},
    {"lowDrive", "Low drive", "dB", 0, 36, 0},
    {"lowShape", "Low shape", "", 0, 1, 0},
    {"lowGain", "Low gain", "dB", -24, 12, 0},
    {"lowMix", "Low mix", "", 0, 1, 1},
    {"lowOn", "Low enabled", "", 0, 1, 1},
    {"midDrive", "Mid drive", "dB", 0, 36, 0},
    {"midShape", "Mid shape", "", 0, 1, 0},
    {"midGain", "Mid gain", "dB", -24, 12, 0},
    {"midMix", "Mid mix", "", 0, 1, 1},
    {"midOn", "Mid enabled", "", 0, 1, 1},
    {"highDrive", "High drive", "dB", 0, 36, 0},
    {"highShape", "High shape", "", 0, 1, 0},
    {"highGain", "High gain", "dB", -24, 12, 0},
    {"highMix", "High mix", "", 0, 1, 1},
    {"highOn", "High enabled", "", 0, 1, 1},
}};
inline constexpr std::array<const char *, 6> options{"mode",     "routing",  "quality",
                                                     "dcFilter", "symmetry", "editSlot"};
inline constexpr std::array<const char *, 4> stagePrefixes{"master", "low", "mid", "high"};
inline constexpr std::array<FloatSpec, 12> stageSpecs{{{"Enabled", "Enabled", "", 0, 1, 0},
                                                       {"Clip", "Clip", "", 0, 1, 1},
                                                       {"Custom", "Shape", "", 0, 1, 0},
                                                       {"Drive", "Drive", "dB", 0, 36, 0},
                                                       {"Threshold", "Threshold", "dB", -36, 0, 0},
                                                       {"Hardness", "Hardness", "", 0, 1, 0},
                                                       {"Knee", "Knee", "", 0, 1, .2f},
                                                       {"Curve", "Curve", "", 0, 4, 3},
                                                       {"Bias", "Bias", "", -.5f, .5f, 0},
                                                       {"Asymmetry", "Asymmetry", "", -1, 1, 0},
                                                       {"Morph", "Morph", "", 0, 1, 0},
                                                       {"Mix", "Stage mix", "", 0, 1, 1}}};
inline constexpr size_t stageStride = 30, stageStart = 44, snapshotCount = 167;
inline juce::String stageId(int stage, const juce::String &suffix)
{
    return juce::String(stagePrefixes[static_cast<size_t>(stage)]) + suffix;
}
inline juce::String pointId(int stage, int slot, int point)
{
    return stageId(stage, juce::String(slot == 0 ? "A" : "B") + juce::String(point));
}
inline juce::StringArray legacyIds()
{
    juce::StringArray result;
    for (auto id : ids)
        result.add(id);
    for (auto spec : extraFloats)
        result.add(spec.id);
    for (auto id : options)
        result.add(id);
    return result;
}
inline juce::StringArray allIds()
{
    auto result = legacyIds();
    for (int stage = 0; stage < 4; ++stage)
    {
        for (auto spec : stageSpecs)
            result.add(stageId(stage, spec.id));
        for (int slot = 0; slot < 2; ++slot)
            for (int point = 0; point < 9; ++point)
                result.add(pointId(stage, slot, point));
    }
    result.add("bandLowHz");
    result.add("bandHighHz");
    result.add("bassEnabled");
    result.add("abMatch");
    return result;
}
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace noclip::parameters
