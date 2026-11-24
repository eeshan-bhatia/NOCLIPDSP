#pragma once
#include "../PluginProcessor.h"
#include "TacticalLookAndFeel.h"
class CurveEditor final : public juce::Component
{
  public:
    explicit CurveEditor(NoclipProcessor &p) : processor(p) {}
    void setStage(int next)
    {
        stage = next;
        repaint();
    }
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;

  private:
    juce::Rectangle<float> plot() const
    {
        return getLocalBounds().toFloat().reduced(12);
    }
    NoclipProcessor &processor;
    int stage = 0, dragged = -1, slot = 0;
    bool mirrored = false;
};
class WorkspacePanel final : public juce::Component
{
  public:
    explicit WorkspacePanel(NoclipProcessor &);
    void setPage(int);
    void resized() override;
    void paint(juce::Graphics &) override;
    void refresh();

  private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    NoclipProcessor &processor;
    CurveEditor editor;
    int page = 0;
    struct Control
    {
        juce::Slider knob;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };
    std::array<Control, 14> controls;
    juce::ToggleButton clip{"CLIP"}, shape{"SHAPE"}, symmetry{"SYMMETRY"};
    juce::ComboBox family, slot, preset;
    juce::TextButton invert{"INVERT"}, copy{"COPY"};
    std::unique_ptr<ButtonAttachment> clipAttachment, shapeAttachment, symmetryAttachment;
    std::unique_ptr<ComboAttachment> familyAttachment, slotAttachment;
    void curvePreset(int);
    void place(int, juce::Rectangle<int>);
};
