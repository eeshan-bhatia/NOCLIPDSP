#pragma once
#include "../PluginProcessor.h"
class SidechainPanel final : public juce::Component,private juce::Timer
{
public:
    explicit SidechainPanel(NoclipProcessor&);
    void resized() override;
private:
    void timerCallback() override;
    NoclipProcessor& processor;
    juce::ToggleButton enabled{"EXTERNAL SIDECHAIN"};
    juce::Label status;
    std::array<juce::Label,3> help;
    std::array<juce::Slider,4> knobs;
    std::array<juce::Label,4> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> attachments;
    juce::AudioProcessorValueTreeState::ButtonAttachment enableAttachment;
};
