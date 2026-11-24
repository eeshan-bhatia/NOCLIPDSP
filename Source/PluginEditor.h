#pragma once
#include "PluginProcessor.h"
#include "UI/TacticalLookAndFeel.h"
#include "UI/MeterBallistics.h"
#include "UI/AdvancedPanel.h"
#include "UI/SidechainPanel.h"
#include <juce_gui_basics/juce_gui_basics.h>
class NoclipEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NoclipEditor(NoclipProcessor&);
    ~NoclipEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    NoclipProcessor& processor;
    noclip::ui::TacticalLookAndFeel look;
    juce::Image icon, sandTexture;
    AdvancedPanel advanced;
    SidechainPanel sidechain;
    std::array<juce::TextButton,4> pages;
    juce::TextButton snapshotA{"A"},snapshotB{"B"},copyAB{"A TO B"},copyBA{"B TO A"};
    juce::ToggleButton match{"RMS MATCH"};
    juce::Label compareStatus;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> matchAttachment;
    bool sidechainView=false, blueTheme=false;
    void applyTheme(bool);

    juce::ComboBox quality;
    juce::ToggleButton clipDc { "DC FILTER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clipDcAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
    int currentPage=-1;
    void updatePage();
    juce::TooltipWindow tooltips { this, 700 };
    std::array<juce::Slider, 8> knobs;
    std::array<juce::Label, 8> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> attachments;
    noclip::ui::MeterBallistics meterBallistics;
    std::array<juce::String, 4> meterReadouts;
    double lastMeterTime = 0, readoutElapsed = 0;
    juce::ToggleButton bypass { "BYPASS" }, enabled { "ACTIVE" };
    juce::ComboBox curve;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment, enabledAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment curveAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoclipEditor)
};
