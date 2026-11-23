#pragma once
#include "../PluginProcessor.h"
#include "TacticalLookAndFeel.h"

class CurveEditor final : public juce::Component
{
public:
    explicit CurveEditor(NoclipProcessor& p) : processor(p) { setTooltipText(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void setTooltipText() { setTitle("Nine point curve editor: drag either half; symmetry lock mirrors edits"); }
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced(12,12); }
    NoclipProcessor& processor;
    int dragged=-1, slot=0;
    bool mirrored=false;
};
class AdvancedPanel final : public juce::Component
{
public:
    explicit AdvancedPanel(NoclipProcessor&);
    void setPage(int);
    void refresh();
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    NoclipProcessor& processor;
    CurveEditor curve;
    int page=1;
    std::array<Control,9> controls;
    std::array<Control,6> shared;
    juce::ComboBox slot, curvePreset, bassPreset, nonlinear;
    juce::ToggleButton symmetry {"SYMMETRY"}, dc {"DC FILTER"};
    bool gainView=false;
    juce::TextButton gainViewButton {"GAIN / MIX"};
    juce::TextButton invert {"INVERT"}, copyAB {"A TO B"}, copyBA {"B TO A"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slotAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> nonlinearAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> symmetryAttachment, dcAttachment;
    void place(size_t, juce::Rectangle<int>);
    void applyCurvePreset(int);
    void copyCurve(int,int);
};
