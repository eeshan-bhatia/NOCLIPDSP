#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace noclip::ui
{
inline const juce::Colour sand { 0xffccba7c }, ink { 0xff0c0f12 }, recess { 0xff413a27 };
inline const juce::Colour amber { 0xffde9b35 }, green { 0xff013220 }, blue { 0xff5d79ae };
inline const juce::Colour light { 0xffe5d69f }, edge { 0xff82764e }, glow { 0xffa2ff57 };

// CPU-rendered vector chrome, with one shared in-memory typeface per editor.
class TacticalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TacticalLookAndFeel();
    juce::Font font(float height) const;
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    static void bevel(juce::Graphics&, juce::Rectangle<float>, bool inset = false);
private:
    juce::Typeface::Ptr typeface;
};
}
