#include "PluginEditor.h"
#include "NoclipArtwork.h"
NoclipEditor::NoclipEditor(NoclipProcessor &p) : AudioProcessorEditor(p), processor(p), workspace(p)
{
    setLookAndFeel(&look);
    icon = juce::ImageCache::getFromMemory(NoclipArtwork::icon_png, NoclipArtwork::icon_pngSize);
    texture = juce::Image(juce::Image::RGB, 256, 256, false);
    addAndMakeVisible(workspace);
    const char *names[]{"MASTER", "LOW", "MID", "HIGH", "808"};
    for (size_t i = 0; i < 5; ++i)
    {
        tabs[i].setButtonText(names[i]);
        tabs[i].onClick = [this, i] { selectPage(static_cast<int>(i)); };
        addAndMakeVisible(tabs[i]);
        enabled[i].setButtonText("ON");
        enabled[i].setName("compact");
        addAndMakeVisible(enabled[i]);
        const auto id = i == 4 ? juce::String("bassEnabled")
                               : noclip::parameters::stageId(static_cast<int>(i), "Enabled");
        enableAttachments[i] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, id, enabled[i]);
        enabled[i].setTooltip("Enable this processor. Viewing a tab never changes audio.");
    }
    for (auto *c : std::array<juce::Component *, 11>{&a, &b, &copyAB, &copyBA, &match, &bypass, &active, &dc,
                                                     &quality, &status, &adaa})
    {
        c->setName("compact");
        addAndMakeVisible(c);
    }
    a.onClick = [this] { processor.selectSnapshot(0); };
    b.onClick = [this] { processor.selectSnapshot(1); };
    copyAB.onClick = [this] { processor.copySnapshot(0, 1); };
    copyBA.onClick = [this] { processor.copySnapshot(1, 0); };
    for (auto pair : std::array<std::pair<const char *, juce::ToggleButton *>, 5>{
             {{"abMatch", &match}, {"bypass", &bypass}, {"clipEnabled", &active}, {"dcFilter", &dc}, {"adaa", &adaa}}})
        buttons.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            p.state, pair.first, *pair.second));
    quality.addItemList({"1X CREATIVE", "2X ECO", "4X NORMAL", "8X HIGH", "16X ULTRA"}, 1);
    qualityAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "quality", quality);
    const char *ids[]{"inputDb", "outputDb", "ceilingDb", "mix", "bandLowHz", "bandHighHz"};
    const char *labelsText[]{"INPUT", "OUTPUT", "CEILING", "GLOBAL MIX", "LOW / MID", "MID / HIGH"};
    for (size_t i = 0; i < 6; ++i)
    {
        auto &k = knobs[i];
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 112, 24);
        labels[i].setText(labelsText[i], juce::dontSendNotification);
        labels[i].setName("compact");
        labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(k);
        addAndMakeVisible(labels[i]);
        attachments[i] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i], k);
        auto *parameter = p.state.getParameter(ids[i]);
        k.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
        k.textFromValueFunction = [i](double v)
        {
            return juce::String(v, i >= 4 ? 0 : 2).replaceCharacter('-', juce::juce_wchar(0x2212)) +
                   (i >= 4 ? " Hz" : (i < 3 ? " dB" : ""));
        };
        k.valueFromTextFunction = [](const juce::String &t)
        { return t.replaceCharacter(juce::juce_wchar(0x2212), '-').getDoubleValue(); };
        k.updateText();
    }
    knobs[4].setTooltip(
        "Low/mid crossover. Filters run only in the audible path when a band processor is enabled.");
    knobs[5].setTooltip("Mid/high crossover. Disabled bands pass through; they are not muted.");
    match.setTooltip(
        "Compare whole configurations using attenuation-only RMS matching. Allow a few seconds to settle.");
    adaa.setTooltip("First-order antiderivative anti-aliasing. Best combined with 2x or higher oversampling; 1x softens high frequencies. Adds CPU load.");
    setResizable(true, true);
    setResizeLimits(1000, 900, 1500, 1200);
    setSize(1100, 950);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    selectPage(0);
    timerCallback();
    startTimerHz(30);
}
NoclipEditor::~NoclipEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
void NoclipEditor::selectPage(int next)
{
    page = std::clamp(next, 0, 4);
    workspace.setPage(page);
    for (int i = 0; i < 5; ++i)
        tabs[i].setToggleState(i == page, juce::dontSendNotification);
    theme();
    resized();
    repaint();
}
void NoclipEditor::theme()
{
    look.setBlueTheme(page == 4);
    juce::Random grain(0x4e434c50);
    {
        juce::Image::BitmapData pixels(texture, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
            {
                int d = grain.nextInt(25) - 12;
                auto c = look.sand;
                pixels.setPixelColour(x, y,
                                      juce::Colour(static_cast<juce::uint8>(c.getRed() + d),
                                                   static_cast<juce::uint8>(c.getGreen() + d),
                                                   static_cast<juce::uint8>(c.getBlue() + d)));
            }
    }
    const auto update = [this](auto &&self, juce::Component &c) -> void
    {
        if (auto *k = dynamic_cast<juce::Slider *>(&c))
        {
            k->setColour(juce::Slider::textBoxTextColourId, look.ink);
            k->setColour(juce::Slider::textBoxBackgroundColourId, look.light);
            k->setColour(juce::Slider::textBoxOutlineColourId, look.edge);
        }
        for (auto *child : c.getChildren())
            self(self, *child);
    };
    update(update, *this);
    sendLookAndFeelChange();
}
void NoclipEditor::resized()
{
    int w = getWidth();
    active.setBounds(w - 260, 30, 110, 28);
    bypass.setBounds(w - 140, 30, 115, 28);
    quality.setBounds(w - 260, 65, 235, 28);
    dc.setBounds(w - 405, 65, 135, 28);
    adaa.setBounds(w - 515, 65, 100, 28);
    const int tw = (w - 48) / 5;
    for (int i = 0; i < 5; ++i)
    {
        tabs[i].setBounds(24 + i * tw, 112, tw - 8, 30);
        enabled[i].setBounds(24 + i * tw, 147, tw - 8, 24);
    }
    a.setBounds(24, 184, 44, 26);
    b.setBounds(76, 184, 44, 26);
    copyAB.setBounds(136, 184, 88, 26);
    copyBA.setBounds(232, 184, 88, 26);
    match.setBounds(340, 184, 154, 26);
    status.setBounds(510, 184, w - 534, 26);
    const int cw = (w - 48) / 6;
    constexpr std::array<size_t, 6> order{3, 0, 1, 2, 4, 5};
    for (size_t position = 0; position < order.size(); ++position)
    {
        const auto index = order[position];
        const int x = 24 + static_cast<int>(position) * cw;
        labels[index].setBounds(x, 225, cw, 22);
        knobs[index].setBounds(x, 247, cw, 124);
    }
    workspace.setBounds(24, 390, w - 48, getHeight() - 470);
}
void NoclipEditor::paint(juce::Graphics &g)
{
    g.setTiledImageFill(texture, 0, 0, 1);
    g.fillAll();
    look.bevel(g, getLocalBounds().toFloat().reduced(3));
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImageWithin(icon, 24, 20, 72, 72, juce::RectanglePlacement::centred);
    g.setColour(noclip::ui::ink);
    g.fillRect(59.5f, 21.2f, 1.25f, 11.f);
    juce::GlyphArrangement title;
    title.addLineOfText(look.font(48), "NOCLIP.DSP", 110, 65);
    juce::Path path;
    title.createPath(path);
    juce::DropShadow(look.glow.withAlpha(.9f), 12, {}).drawForPath(g, path);
    g.setColour(look.ink);
    g.fillPath(path);
    // Render 3-BAND with a plain dash; the supplied font maps '-' to artwork.
    juce::GlyphArrangement subtitle;
    subtitle.addFittedText(look.font(13), "3  BAND WAVESHAPER WITH 808 FX", 112, 76, 460, 20,
                          juce::Justification::centredLeft, 1);
    juce::Path subtitlePath;
    subtitle.createPath(subtitlePath);
    g.fillPath(subtitlePath);
    g.drawLine(subtitle.getGlyph(0).getRight() + 1, subtitlePath.getBounds().getCentreY(),
               subtitle.getGlyph(3).getLeft() - 1, subtitlePath.getBounds().getCentreY(), 1.2f);
    g.setColour(look.edge);
    g.drawHorizontalLine(380, 24.f, static_cast<float>(getWidth() - 24));
    const char *names[]{"IN", "OUT", "RMS", "CLIP"};
    int cw = (getWidth() - 48) / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto box = juce::Rectangle<int>(24 + i * cw, getHeight() - 66, cw - 4, 30);
        g.setColour(look.light);
        g.fillRect(box);
        g.setColour(look.ink);
        g.setFont(look.font(12));
        g.drawText(names[i], box.removeFromLeft(55), juce::Justification::centred);
        const auto digits = readings[i].paddedLeft(' ', 6);
        const int x = box.getX() + 4;
        for (int j = 0; j < 6; ++j)
        {
            const auto digit = digits.substring(j, j + 1);
            if (digit == juce::String::charToString(juce::juce_wchar(0x2212)))
                g.fillRect(x + j * 12 + 2, box.getCentreY(), 7, 2);
            else
                g.drawText(digit, x + j * 12, box.getY(), 12, box.getHeight(), juce::Justification::centred,
                           false);
        }
        g.setFont(look.font(10));
        g.drawText(i == 3 ? "%" : "dBFS", x + 78, box.getY(), 52, box.getHeight(),
                   juce::Justification::centredLeft);
    }
    g.setColour(look.ink);
    g.setFont(look.font(12));
    g.drawText(juce::String(1 << processor.actualQuality.load()) + "X / " +
                   juce::String(processor.getLatencySamples()) + " SAMPLES / SAMPLE PEAK CEILING",
               24, getHeight() - 29, getWidth() - 48, 20, juce::Justification::centred);
}
void NoclipEditor::timerCallback()
{
    a.setToggleState(processor.selectedSnapshot() == 0, juce::dontSendNotification);
    b.setToggleState(processor.selectedSnapshot() == 1, juce::dontSendNotification);
    status.setText(
        processor.comparing.load()
            ? "SWITCHING"
            : (match.getToggleState()
                   ? (processor.matchReady.load()
                          ? "MATCH TRIM " + juce::String(processor.matchReduction.load(), 1) + " dB"
                          : "MATCH LEARNING / PLAY AUDIO")
                   : "WHOLE PLUGIN A / B"),
        juce::dontSendNotification);
    workspace.refresh();
    double now = juce::Time::getMillisecondCounterHiRes(), dt = (now - lastTime) / 1000;
    lastTime = now;
    elapsed += dt;
    meters.update(processor.inputPeak.load(), processor.outputPeak.load(), processor.outputRms.load(),
                  processor.clippingActivity.load(), dt);
    if (elapsed >= .125 || readings[0].isEmpty())
    {
        elapsed = 0;
        auto v = meters.values();
        for (size_t i = 0; i < 4; ++i)
            readings[i] = juce::String(v[i], i == 3 ? 0 : 1).replaceCharacter('-', juce::juce_wchar(0x2212));
        repaint(24, getHeight() - 70, getWidth() - 48, 70);
    }
}
