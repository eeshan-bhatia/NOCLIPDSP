#include "ParameterLayout.h"
namespace noclip::parameters
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto &spec : floats)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{spec.id, 1}, spec.name,
            juce::NormalisableRange<float>{spec.min, spec.max, spec.max <= 1 ? 0.001f : 0.01f}, spec.initial,
            juce::AudioParameterFloatAttributes().withLabel(spec.unit)));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{bypass, 1}, "Bypass", false));
    layout.add(
        std::make_unique<juce::AudioParameterBool>(juce::ParameterID{clipEnabled, 1}, "Clip enabled", true));
    layout.add(
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{curve, 1}, "Curve", curveNames(), 3));
    for (size_t i = 0; i < extraFloats.size(); ++i)
    {
        const auto &spec = extraFloats[i];
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{spec.id, 1}, spec.name,
            juce::NormalisableRange<float>{spec.min, spec.max, spec.max <= 1 ? .001f : .01f}, spec.initial,
            juce::AudioParameterFloatAttributes().withLabel(spec.unit).withAutomatable(i < 9)));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Nonlinear mode",
                                                            juce::StringArray{"CLIP", "SHAPE"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"routing", 1}, "Routing",
                                                            juce::StringArray{"FULL BAND", "808 ENGINE"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"quality", 1}, "Oversampling",
        juce::StringArray{"1X CREATIVE", "2X ECO", "4X NORMAL", "8X HIGH"}, 2));
    layout.add(
        std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"dcFilter", 1}, "DC filter", true));
    layout.add(
        std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"symmetry", 1}, "Symmetry lock", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"editSlot", 1}, "Edit curve",
                                                            juce::StringArray{"A", "B"}, 0));
    for (int stage = 0; stage < 4; ++stage)
    {
        for (size_t i = 0; i < stageSpecs.size(); ++i)
        {
            const auto spec = stageSpecs[i];
            const auto id = stageId(stage, spec.id);
            const auto name = juce::String(stagePrefixes[stage]) + " " + spec.name;
            if (i < 3)
                layout.add(std::make_unique<juce::AudioParameterBool>(
                    juce::ParameterID{id, 1}, name, i == 0 ? stage == 0 : spec.initial > .5f));
            else if (i == 7)
                layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name,
                                                                        curveNames(), 3));
            else
                layout.add(std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>{spec.min, spec.max, .001f},
                    spec.initial, juce::AudioParameterFloatAttributes().withLabel(spec.unit)));
        }
        for (int slot = 0; slot < 2; ++slot)
            for (int point = 0; point < 9; ++point)
                layout.add(std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID{pointId(stage, slot, point), 1}, "Curve point",
                    juce::NormalisableRange<float>{-1, 1, .001f}, static_cast<float>(point - 4) / 4,
                    juce::AudioParameterFloatAttributes().withAutomatable(false)));
    }
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"bandLowHz", 1}, "Low crossover",
                                                           juce::NormalisableRange<float>{40, 1000, 1},
                                                           120.f));
    layout.add(
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"bandHighHz", 1}, "High crossover",
                                                    juce::NormalisableRange<float>{1200, 12000, 1}, 2500.f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bassEnabled", 1}, "808 enabled",
                                                          false));
    layout.add(
        std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"abMatch", 1}, "A/B RMS match", false));
    return layout;
}
} // namespace noclip::parameters
