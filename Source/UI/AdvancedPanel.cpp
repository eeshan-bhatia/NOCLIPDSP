#include "AdvancedPanel.h"
namespace
{
float raw(NoclipProcessor &p, const char *id)
{
    return p.state.getRawParameterValue(id)->load();
}
} // namespace
void CurveEditor::paint(juce::Graphics &g)
{
    using noclip::ui::TacticalLookAndFeel;
    const auto &theme = static_cast<TacticalLookAndFeel &>(getLookAndFeel());
    const auto recess = theme.recess, sand = theme.sand, amber = theme.amber, blue = theme.blue,
               glow = theme.glow;
    g.fillAll(recess);
    const auto area = plot();
    g.setColour(sand.withAlpha(.2f));
    for (int i = 0; i < 9; ++i)
        g.drawVerticalLine(static_cast<int>(area.getX() + area.getWidth() * static_cast<float>(i) / 8),
                           area.getY(), area.getBottom());
    g.drawHorizontalLine(static_cast<int>(area.getCentreY()), area.getX(), area.getRight());
    const int selected = static_cast<int>(raw(processor, "editSlot"));
    const auto tables = processor.readCurveTable(stage);
    for (int bank = 0; bank < 2; ++bank)
    {
        const auto &table = bank == 0 ? tables.a : tables.b;
        juce::Path line;
        for (size_t i = 0; i < noclip::CurveTable::size; i += 8)
        {
            const float x = area.getX() + area.getWidth() * static_cast<float>(i) / 2048;
            const float y = area.getCentreY() - table[i] * area.getHeight() * .5f;
            if (i == 0)
                line.startNewSubPath(x, y);
            else
                line.lineTo(x, y);
        }
        g.setColour(bank == selected ? amber : blue.withAlpha(.45f));
        g.strokePath(line, juce::PathStrokeType(bank == selected ? 2.5f : 1));
    }
    for (int i = 0; i < 9; ++i)
    {
        const auto id = noclip::parameters::pointId(stage, selected, i);
        const float y = processor.state.getRawParameterValue(id)->load();
        const float px = area.getX() + area.getWidth() * static_cast<float>(i) / 8;
        const float py = area.getCentreY() - y * area.getHeight() * .5f;
        g.setColour(glow.withAlpha(.2f));
        g.fillEllipse(px - 7, py - 7, 14, 14);
        g.setColour(amber);
        g.fillEllipse(px - 4, py - 4, 8, 8);
    }
}
void CurveEditor::mouseDown(const juce::MouseEvent &e)
{
    slot = static_cast<int>(raw(processor, "editSlot"));
    dragged = std::clamp(
        static_cast<int>(std::lround((e.position.x - plot().getX()) / plot().getWidth() * 8)), 0, 8);
    mirrored = raw(processor, "symmetry") > .5f;
    processor.state.getParameter(noclip::parameters::pointId(stage, slot, dragged))->beginChangeGesture();
    if (mirrored && dragged != 4)
        processor.state.getParameter(noclip::parameters::pointId(stage, slot, 8 - dragged))
            ->beginChangeGesture();
    mouseDrag(e);
}
void CurveEditor::mouseDrag(const juce::MouseEvent &e)
{
    if (dragged < 0)
        return;
    float y = std::clamp((plot().getCentreY() - e.position.y) * 2 / plot().getHeight(), -1.0f, 1.0f);
    if (mirrored && dragged == 4)
        y = 0;
    auto *p = processor.state.getParameter(noclip::parameters::pointId(stage, slot, dragged));
    p->setValueNotifyingHost(p->convertTo0to1(y));
    if (mirrored && dragged != 4)
    {
        p = processor.state.getParameter(noclip::parameters::pointId(stage, slot, 8 - dragged));
        p->setValueNotifyingHost(p->convertTo0to1(-y));
    }
    repaint();
}
void CurveEditor::mouseUp(const juce::MouseEvent &)
{
    if (dragged < 0)
        return;
    processor.state.getParameter(noclip::parameters::pointId(stage, slot, dragged))->endChangeGesture();
    if (mirrored && dragged != 4)
        processor.state.getParameter(noclip::parameters::pointId(stage, slot, 8 - dragged))
            ->endChangeGesture();
    dragged = -1;
}

WorkspacePanel::WorkspacePanel(NoclipProcessor &p) : processor(p), editor(p)
{
    for (auto &c : controls)
    {
        c.knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        c.knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 110, 24);
        c.label.setName("compact");
        c.label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(c.knob);
        addAndMakeVisible(c.label);
    }
    for (auto *c : std::array<juce::Component *, 9>{&editor, &clip, &shape, &symmetry, &family, &slot,
                                                    &preset, &invert, &copy})
    {
        c->setName("compact");
        addAndMakeVisible(c);
    }
    family.addItemList(noclip::parameters::curveNames(), 1);
    slot.addItemList({"CURVE 1", "CURVE 2"}, 1);
    preset.addItemList({"LINEAR", "SOFT", "HARD", "FOLD"}, 1);
    preset.setTextWhenNothingSelected("PRESET");
    symmetryAttachment = std::make_unique<ButtonAttachment>(p.state, "symmetry", symmetry);
    slotAttachment = std::make_unique<ComboAttachment>(p.state, "editSlot", slot);
    preset.onChange = [this]
    {
        if (preset.getSelectedId() > 0)
            curvePreset(preset.getSelectedItemIndex());
        preset.setSelectedId(0, juce::dontSendNotification);
    };
    invert.onClick = [this]
    {
        const int bank = slot.getSelectedItemIndex();
        for (int i = 0; i < 9; ++i)
        {
            auto id = noclip::parameters::pointId(page, bank, i);
            processor.setParameter(id.toRawUTF8(), -processor.state.getRawParameterValue(id)->load());
        }
    };
    copy.onClick = [this]
    {
        const int bank = slot.getSelectedItemIndex();
        for (int i = 0; i < 9; ++i)
        {
            auto id = noclip::parameters::pointId(page, 1 - bank, i);
            processor.setParameter(
                id.toRawUTF8(),
                processor.state.getRawParameterValue(noclip::parameters::pointId(page, bank, i))->load());
        }
    };
    copy.setTooltip(
        "Copy the selected curve to the other endpoint in this section. Whole-plugin A/B is separate.");
    clip.setTooltip("Analytical clipping first. SHAPE, when enabled, follows it.");
    shape.setTooltip("Custom curve after clipping. Can be used alone by turning CLIP off.");
    setPage(0);
}
void WorkspacePanel::setPage(int next)
{
    page = std::clamp(next, 0, 4);
    if (page < 4)
        editor.setStage(page);
    for (auto *c : std::array<juce::Component *, 9>{&editor, &clip, &shape, &symmetry, &family, &slot,
                                                    &preset, &invert, &copy})
        c->setVisible(page < 4);
    clipAttachment.reset();
    shapeAttachment.reset();
    familyAttachment.reset();
    if (page < 4)
    {
        clipAttachment = std::make_unique<ButtonAttachment>(processor.state,
                                                            noclip::parameters::stageId(page, "Clip"), clip);
        shapeAttachment = std::make_unique<ButtonAttachment>(
            processor.state, noclip::parameters::stageId(page, "Custom"), shape);
        familyAttachment = std::make_unique<ComboAttachment>(
            processor.state, noclip::parameters::stageId(page, "Curve"), family);
    }
    constexpr int indices[]{3, 4, 5, 6, 11, 8, 9, 10};
    for (size_t i = 0; i < controls.size(); ++i)
    {
        auto &c = controls[i];
        c.attachment.reset();
        const bool visible = page == 4 ? i >= 8 : i < 8;
        c.knob.setVisible(visible);
        c.label.setVisible(visible);
        if (!visible)
            continue;
        const auto spec =
            i < 8 ? noclip::parameters::stageSpecs[indices[i]] : noclip::parameters::extraFloats[i - 8 + 3];
        const auto id = i < 8 ? noclip::parameters::stageId(page, spec.id) : juce::String(spec.id);
        c.label.setText(juce::String(spec.name).toUpperCase(), juce::dontSendNotification);
        c.knob.setName(spec.name);
        c.knob.setDoubleClickReturnValue(true, spec.initial);
        c.attachment = std::make_unique<SliderAttachment>(processor.state, id, c.knob);
        c.knob.textFromValueFunction = [unit = juce::String(spec.unit)](double v)
        {
            return juce::String(v, unit == "Hz" ? 0 : 2).replaceCharacter('-', juce::juce_wchar(0x2212)) +
                   (unit.isEmpty() ? juce::String() : " " + unit);
        };
        c.knob.valueFromTextFunction = [](const juce::String &t)
        { return t.replaceCharacter(juce::juce_wchar(0x2212), '-').getDoubleValue(); };
        c.knob.updateText();
    }
    resized();
    refresh();
}
void WorkspacePanel::curvePreset(int presetIndex)
{
    if (page > 3)
        return;
    const int bank = slot.getSelectedItemIndex();
    for (int i = 0; i < 9; ++i)
    {
        const float x = static_cast<float>(i - 4) / 4;
        float y = x;
        if (presetIndex == 1)
            y = std::tanh(x * 2) / std::tanh(2.f);
        if (presetIndex == 2)
            y = std::clamp(x * 3, -1.f, 1.f);
        if (presetIndex == 3)
            y = std::sin(x * std::numbers::pi_v<float>);
        auto id = noclip::parameters::pointId(page, bank, i);
        processor.setParameter(id.toRawUTF8(), y);
    }
}
void WorkspacePanel::place(int index, juce::Rectangle<int> area)
{
    area = area.reduced(5, 2);
    controls[index].label.setBounds(area.removeFromTop(22));
    controls[index].knob.setBounds(area);
}
void WorkspacePanel::resized()
{
    const int w = getWidth(), h = getHeight();
    if (page == 4)
    {
        const int knobArea = w * 60 / 100;
        const int cw = std::min(200, (knobArea - 12) / 3), ch = std::min(190, (h - 16) / 2);
        const int left = (knobArea - 3 * cw) / 2, top = (h - 2 * ch) / 2;
        for (int i = 0; i < 6; ++i)
            place(i + 8, {left + i % 3 * cw, top + i / 3 * ch, cw, ch});
        return;
    }
    const int left = w * 45 / 100, right = w - left - 18;
    clip.setBounds(8, 8, 100, 28);
    family.setBounds(116, 8, left - 124, 28);
    shape.setBounds(left + 18, 8, 100, 28);
    symmetry.setBounds(w - 150, 8, 140, 28);
    const int cw = left / 3, ch = (h - 144) / 2;
    for (int i = 0; i < 3; ++i)
        place(i, {i * cw, 144, cw, ch});
    place(3, {cw / 2, 144 + ch, cw, ch});
    place(4, {cw + cw / 2, 144 + ch, cw, ch});
    slot.setBounds(left + 18, 46, 130, 26);
    preset.setBounds(left + 156, 46, 118, 26);
    invert.setBounds(left + 282, 46, 90, 26);
    copy.setBounds(left + 380, 46, 80, 26);
    const int graphHeight = std::max(110, (h - 84) * 52 / 100);
    editor.setBounds(left + 18, 80, right, graphHeight);
    for (int i = 0; i < 3; ++i)
        place(5 + i, {left + 18 + i * right / 3, 84 + graphHeight, right / 3, h - 84 - graphHeight});
}
void WorkspacePanel::refresh()
{
    if (page < 4)
    {
        controls[2].knob.setEnabled(clip.getToggleState());
        controls[3].knob.setEnabled(clip.getToggleState() && family.getSelectedItemIndex() == 3 &&
                                    controls[2].knob.getValue() < 1);
        for (int i = 5; i < 8; ++i)
            controls[i].knob.setEnabled(shape.getToggleState());
    }
    editor.repaint();
    repaint();
}
void WorkspacePanel::paint(juce::Graphics &g)
{
    auto &look = static_cast<noclip::ui::TacticalLookAndFeel &>(getLookAndFeel());
    look.bevel(g, getLocalBounds().toFloat(), true);
    if (page >= 4)
    {
        drawBassScope(g);
        return;
    }
    const int left = getWidth() * 45 / 100;
    g.setColour(look.edge);
    g.drawVerticalLine(left + 8, 4.f, static_cast<float>(getHeight() - 4));
    juce::Rectangle<float> graph(10, 46, static_cast<float>(left - 20), 88);
    g.setColour(look.recess);
    g.fillRect(graph);
    g.setColour(look.sand.withAlpha(.2f));
    g.drawHorizontalLine(static_cast<int>(graph.getCentreY()), graph.getX(), graph.getRight());
    const float drive = noclip::dbToGain(static_cast<float>(controls[0].knob.getValue())),
                threshold = noclip::dbToGain(static_cast<float>(controls[1].knob.getValue()));
    juce::Path path;
    for (int i = 0; i <= 256; ++i)
    {
        double x = -2 + 4. * i / 256;
        double y = clip.getToggleState()
                       ? noclip::clip(x * drive, threshold, static_cast<float>(controls[2].knob.getValue()),
                                      static_cast<float>(controls[3].knob.getValue()),
                                      static_cast<noclip::Curve>(std::max(0, family.getSelectedItemIndex())))
                       : x;
        float px = graph.getX() + graph.getWidth() * static_cast<float>(i) / 256,
              py = graph.getCentreY() - static_cast<float>(std::clamp(y, -1., 1.)) * 38;
        if (i == 0)
            path.startNewSubPath(px, py);
        else
            path.lineTo(px, py);
    }
    g.setColour(look.amber.withAlpha(.15f));
    g.strokePath(path, juce::PathStrokeType(8));
    g.setColour(look.amber);
    g.strokePath(path, juce::PathStrokeType(2));
}

void WorkspacePanel::drawBassScope(juce::Graphics &g)
{
    auto &look = static_cast<noclip::ui::TacticalLookAndFeel &>(getLookAndFeel());
    const int left = getWidth() * 60 / 100 + 12;
    auto panel =
        juce::Rectangle<float>(static_cast<float>(left), 18.f, static_cast<float>(getWidth() - left - 18),
                               static_cast<float>(getHeight() - 36));
    g.setColour(look.recess);
    g.fillRect(panel);
    look.bevel(g, panel, true);
    g.setColour(look.ink);
    g.setFont(look.font(12));
    g.drawText("808 SIGNAL MONITOR", panel.toNearestInt().withHeight(32), juce::Justification::centred);
    auto graph = panel.reduced(12, 0).withTrimmedTop(48).withTrimmedBottom(52).withTrimmedRight(78);
    for (int i = 0; i <= 8; ++i)
    {
        g.setColour(look.glow.withAlpha(i == 4 ? .18f : .07f));
        const float x = graph.getX() + graph.getWidth() * static_cast<float>(i) / 8;
        g.drawVerticalLine(static_cast<int>(x), graph.getY(), graph.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        g.setColour(look.glow.withAlpha(i == 2 ? .25f : .09f));
        const float y = graph.getY() + graph.getHeight() * static_cast<float>(i) / 4;
        g.drawHorizontalLine(static_cast<int>(y), graph.getX(), graph.getRight());
    }
    constexpr size_t length = NoclipProcessor::scopeSize, visible = 1024;
    const size_t head = processor.scopeHead.load(std::memory_order_acquire);
    // A rising zero crossing steadies sustained bass. This is a visual hint,
    // not a sample-accurate triggered measurement.
    size_t start = 128;
    for (size_t i = 128; i < length - visible; ++i)
    {
        const float previous = processor.scopeOut[(head + i - 1) % length].load(std::memory_order_relaxed),
                    next = processor.scopeOut[(head + i) % length].load(std::memory_order_relaxed);
        if (previous <= 0 && next > 0)
        {
            start = i;
            break;
        }
    }
    const size_t delay = static_cast<size_t>(std::clamp(processor.getLatencySamples(), 0, 127));
    g.setFont(look.font(11));
    g.setColour(look.sand.brighter(.3f));
    g.drawText("IN",
               panel.toNearestInt()
                   .withY(static_cast<int>(panel.getBottom() - 45))
                   .withHeight(20)
                   .withTrimmedRight(static_cast<int>(panel.getWidth() / 2)),
               juce::Justification::centred);
    g.setColour(look.glow);
    g.drawText("OUT",
               panel.toNearestInt()
                   .withY(static_cast<int>(panel.getBottom() - 45))
                   .withHeight(20)
                   .withTrimmedLeft(static_cast<int>(panel.getWidth() / 2)),
               juce::Justification::centred);
    g.setColour(look.ink.withAlpha(.7f));
    g.setFont(look.font(10));
    g.drawText("LOW END / " + juce::String(static_cast<int>(processor.readParameters().advanced.splitHz)) +
                   " Hz",
               panel.toNearestInt().withY(static_cast<int>(panel.getBottom() - 24)).withHeight(20),
               juce::Justification::centred);
    const float level = processor.lowEndPeak.load(std::memory_order_relaxed);
    const float db = 20 * std::log10(std::max(1e-6f, level));
    const float normal = std::clamp((db + 60) / 60, 0.f, 1.f);
    const float meterX = graph.getRight() + 18, meterW = 18, meterTop = graph.getY() + 24,
                meterHeight = graph.getHeight() - 26;
    const bool godmode = db >= -3;
    g.setFont(look.font(10));
    g.setColour(godmode ? look.glow : look.ink.withAlpha(.45f));
    g.drawText("GODMODE", static_cast<int>(graph.getRight() + 3), static_cast<int>(graph.getY()), 76, 20,
               juce::Justification::centred);
    for (int i = 0; i < 20; ++i)
    {
        const float segmentH = meterHeight / 20;
        auto segment =
            juce::Rectangle<float>(meterX, meterTop + meterHeight - (i + 1) * segmentH, meterW, segmentH - 2);
        const bool lit = normal > static_cast<float>(i) / 20;
        if (lit)
        {
            g.setColour(look.glow.withAlpha(.13f));
            g.fillRect(segment.expanded(3));
        }
        g.setColour(lit ? look.glow.withAlpha(.45f + .55f * static_cast<float>(i) / 19)
                        : look.sand.withAlpha(.12f));
        g.fillRect(segment);
    }
    g.setColour(look.ink.withAlpha(.65f));
    g.setFont(look.font(10));
    for (int tick = 0; tick < 4; ++tick)
    {
        const int value = -tick * 20;
        const float y = meterTop + meterHeight * static_cast<float>(tick) / 3;
        g.drawText(juce::String(value).replaceCharacter('-', juce::juce_wchar(0x2212)),
                   static_cast<int>(meterX + 23), static_cast<int>(y - 8), 32, 16,
                   juce::Justification::centredLeft);
    }
    juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(graph.toNearestInt());
    for (int trace = 0; trace < 2; ++trace)
    {
        juce::Path path;
        for (size_t i = 0; i < visible; i += 2)
        {
            const size_t index = (head + start + i + length - (trace == 0 ? delay : 0)) % length;
            float value = (trace == 0 ? processor.scopeIn[index] : processor.scopeOut[index])
                              .load(std::memory_order_relaxed);
            if (!std::isfinite(value))
                value = 0;
            const float x = graph.getX() +
                            graph.getWidth() * static_cast<float>(i) / static_cast<float>(visible - 2),
                        y = graph.getCentreY() - std::clamp(value, -1.f, 1.f) * graph.getHeight() * .44f;
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        if (trace == 1)
        {
            g.setColour(look.glow.withAlpha(.06f));
            g.strokePath(path, juce::PathStrokeType(15));
            g.setColour(look.glow.withAlpha(.16f));
            g.strokePath(path, juce::PathStrokeType(7));
        }
        g.setColour(trace == 0 ? look.sand.brighter(.3f).withAlpha(.6f) : look.glow);
        g.strokePath(path, juce::PathStrokeType(trace == 0 ? 1.f : 2.f));
    }
}
