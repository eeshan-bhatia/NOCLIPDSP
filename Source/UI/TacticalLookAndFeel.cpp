#include "TacticalLookAndFeel.h"
#if NOCLIP_CUSTOM_FONT
#include <NoclipFontData.h>
#endif

namespace noclip::ui
{
TacticalLookAndFeel::TacticalLookAndFeel()
{
#if NOCLIP_CUSTOM_FONT
    int size = 0;
    const auto* data = NoclipFontData::getNamedResource(NoclipFontData::namedResourceList[0], size);
    typeface = juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));
#endif
    setColour(juce::Label::textColourId, ink);
    setColour(juce::Slider::textBoxTextColourId, ink);
    setColour(juce::Slider::textBoxBackgroundColourId, light);
    setColour(juce::Slider::textBoxOutlineColourId, edge);
    setColour(juce::TextEditor::backgroundColourId, light);
    setColour(juce::TextEditor::textColourId, ink);
    setColour(juce::TextEditor::highlightColourId, blue.withAlpha(0.4f));
    setColour(juce::TextEditor::highlightedTextColourId, ink);
    setColour(juce::ComboBox::backgroundColourId, light);
    setColour(juce::ComboBox::textColourId, ink);
    setColour(juce::ComboBox::outlineColourId, edge);
    setColour(juce::PopupMenu::backgroundColourId, sand);
    setColour(juce::PopupMenu::textColourId, ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, blue);
    setColour(juce::PopupMenu::highlightedTextColourId, ink);
    setColour(juce::TooltipWindow::backgroundColourId, light);
    setColour(juce::TooltipWindow::textColourId, ink);
    setColour(juce::TooltipWindow::outlineColourId, edge);
}
juce::Font TacticalLookAndFeel::font(float height) const
{
    return typeface != nullptr ? juce::Font(juce::FontOptions(typeface).withHeight(height * 0.75f)
                                          .withHorizontalScale(0.85f).withFallbacks({ "Consolas" }))
                               : juce::Font(juce::FontOptions("Consolas", height, juce::Font::bold));
}
juce::Typeface::Ptr TacticalLookAndFeel::getTypefaceForFont(const juce::Font& requested)
{
    return typeface != nullptr ? typeface : juce::LookAndFeel_V4::getTypefaceForFont(requested);
}
juce::Font TacticalLookAndFeel::getLabelFont(juce::Label& label)
{
    return font(label.getName() == "compact" ? 13.0f : (label.getName() == "meters" ? 15.0f : 18.0f));
}
juce::Font TacticalLookAndFeel::getComboBoxFont(juce::ComboBox& box) { return font(box.getName()=="compact"?13.0f:18.0f); }
juce::Font TacticalLookAndFeel::getPopupMenuFont() { return font(18); }
juce::Font TacticalLookAndFeel::getTextButtonFont(juce::TextButton&, int) { return font(18); }
juce::Label* TacticalLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::textColourId, ink);
    label->setColour(juce::Label::backgroundColourId, light);
    label->setColour(juce::Label::outlineColourId, edge);
    label->setColour(juce::TextEditor::textColourId, ink);
    label->setColour(juce::TextEditor::backgroundColourId, light);
    return label;
}

void TacticalLookAndFeel::bevel(juce::Graphics& g, juce::Rectangle<float> r, bool inset)
{
    g.setColour(inset ? edge : light);
    g.drawLine(r.getX(), r.getY(), r.getRight(), r.getY());
    g.drawLine(r.getX(), r.getY(), r.getX(), r.getBottom());
    g.setColour(inset ? light : edge);
    g.drawLine(r.getX(), r.getBottom(), r.getRight(), r.getBottom());
    g.drawLine(r.getRight(), r.getY(), r.getRight(), r.getBottom());
}
void TacticalLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float position, float start, float end, juce::Slider& slider)
{
    const float radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.5f - 10.0f;
    if (radius < 8) return;
    const juce::Point<float> centre(static_cast<float>(x) + static_cast<float>(width) * 0.5f,
                                   static_cast<float>(y) + static_cast<float>(height) * 0.5f);
    const float angle = start + position * (end - start);
    const auto disk = juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre);
    juce::Graphics::ScopedSaveState save(g);
    if (!slider.isEnabled()) g.setOpacity(0.4f);
    // Mechanical tick marks and a recessed olive dial face.
    for (int i = 0; i <= 20; ++i)
    {
        const float a = start + static_cast<float>(i) / 20.0f * (end - start);
        const auto outer = centre.getPointOnCircumference(radius + 6, a);
        const auto inner = centre.getPointOnCircumference(radius + (i % 5 == 0 ? 0 : 3), a);
        g.setColour(edge); g.drawLine({ inner, outer }, i % 5 == 0 ? 1.5f : 1.0f);
    }
    g.setColour(recess); g.fillEllipse(disk);
    g.setColour(ink.withAlpha(0.65f)); g.drawEllipse(disk, 2);
    g.setColour(light.withAlpha(0.15f)); g.drawEllipse(disk.reduced(3), 1);
    juce::Path track, fill;
    const float arcRadius = radius - 8;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, end, true);
    fill.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, angle, true);
    g.setColour(ink.withAlpha(0.5f)); g.strokePath(track, juce::PathStrokeType(9));
    // Broad falloff around the solid arc; no pointer or stripe inside the fill.
    g.setColour(glow.withAlpha(0.055f)); g.strokePath(fill, juce::PathStrokeType(35));
    g.setColour(glow.withAlpha(0.10f)); g.strokePath(fill, juce::PathStrokeType(27));
    g.setColour(glow.withAlpha(0.19f)); g.strokePath(fill, juce::PathStrokeType(20));
    g.setColour(glow.withAlpha(0.30f)); g.strokePath(fill, juce::PathStrokeType(14));
    g.setColour(green); g.strokePath(fill, juce::PathStrokeType(9));
}
void TacticalLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced(1);
    g.setColour(down ? edge : (highlighted ? light : sand)); g.fillRect(r);
    bevel(g, r, down);
    auto led = juce::Rectangle<float>(8, 8).withPosition(10, r.getCentreY() - 4);
    if (button.getToggleState())
    {
        g.setColour(glow.withAlpha(0.2f)); g.fillRect(led.expanded(3));
        g.setColour(green);
    }
    else g.setColour(edge);
    g.fillRect(led);
    g.setColour(ink); g.setFont(font(button.getName()=="compact"?13.0f:15.0f));
    g.drawText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(25), juce::Justification::centredLeft);
}
void TacticalLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down,
                                      int, int, int, int, juce::ComboBox&)
{
    juce::Rectangle<float> box(0, 0, static_cast<float>(width), static_cast<float>(height));
    g.setColour(light); g.fillRect(box);
    bevel(g, box.reduced(1), true);
    auto arrow = box.removeFromRight(25).reduced(2);
    g.setColour(down ? edge : sand); g.fillRect(arrow); bevel(g, arrow, down);
    juce::Path triangle;
    const auto centre = arrow.getCentre();
    triangle.addTriangle(centre.x - 4, centre.y - 2, centre.x + 4, centre.y - 2, centre.x, centre.y + 3);
    g.setColour(ink); g.fillPath(triangle);
}
}
