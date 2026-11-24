#include "ParameterLayout.h"
namespace noclip::parameters
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& spec : floats)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { spec.id, 1 }, spec.name,
            juce::NormalisableRange<float> { spec.min, spec.max, spec.max <= 1 ? 0.001f : 0.01f },
            spec.initial, juce::AudioParameterFloatAttributes().withLabel(spec.unit)));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { bypass, 1 }, "Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { clipEnabled, 1 }, "Clip enabled", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { curve, 1 }, "Curve", curveNames(), 3));
    for(size_t i=0;i<extraFloats.size();++i)
    {
        const auto& spec=extraFloats[i];
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{spec.id,1},spec.name,
                juce::NormalisableRange<float>{spec.min,spec.max,spec.max<=1?.001f:.01f},spec.initial,
                juce::AudioParameterFloatAttributes().withLabel(spec.unit).withAutomatable(i<9)));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Nonlinear mode",juce::StringArray{"CLIP","SHAPE"},0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"routing",1},"Routing",juce::StringArray{"FULL BAND","808 ENGINE"},0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality",1},"Oversampling",juce::StringArray{"1X CREATIVE","2X ECO","4X NORMAL","8X HIGH"},2));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"dcFilter",1},"DC filter",true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"symmetry",1},"Symmetry lock",true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"editSlot",1},"Edit curve",juce::StringArray{"A","B"},0));
    for(const auto& spec:sidechainFloats)
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{spec.id,1},spec.name,
            juce::NormalisableRange<float>{spec.min,spec.max,.01f},spec.initial,
            juce::AudioParameterFloatAttributes().withLabel(spec.unit)));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"scEnabled",1},"External sidechain",false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"abMatch",1},"A/B RMS match",false));
    return layout;
}
}
