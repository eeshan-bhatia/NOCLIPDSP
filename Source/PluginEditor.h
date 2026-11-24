#pragma once
#include "PluginProcessor.h"
#include "UI/AdvancedPanel.h"
#include "UI/MeterBallistics.h"
class NoclipEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit NoclipEditor(NoclipProcessor &);
    ~NoclipEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    void selectPage(int);
    int selectedPage() const noexcept
    {
        return page;
    }

  private:
    void timerCallback() override;
    void theme();
    NoclipProcessor &processor;
    noclip::ui::TacticalLookAndFeel look;
    WorkspacePanel workspace;
    juce::Image icon, texture;
    int page = 0;
    std::array<juce::TextButton, 5> tabs;
    std::array<juce::ToggleButton, 5> enabled;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 5> enableAttachments;
    juce::TextButton a{"A"}, b{"B"}, copyAB{"A TO B"}, copyBA{"B TO A"};
    juce::ToggleButton match{"RMS MATCH"}, bypass{"BYPASS"}, active{"ACTIVE"}, dc{"DC FILTER"};
    juce::ComboBox quality;
    juce::Label status;
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> attachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttons;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
    noclip::ui::MeterBallistics meters;
    std::array<juce::String, 4> readings;
    double lastTime = 0, elapsed = 0;
    juce::TooltipWindow tooltips{this, 700};
};
