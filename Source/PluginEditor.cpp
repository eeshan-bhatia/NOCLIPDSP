#include "PluginEditor.h"
#include "NoclipArtwork.h"
NoclipEditor::NoclipEditor(NoclipProcessor& p)
    : AudioProcessorEditor(p), processor(p), advanced(p), sidechain(p),
      bypassAttachment(p.state, noclip::parameters::bypass, bypass),
      enabledAttachment(p.state, noclip::parameters::clipEnabled, enabled),
      curveAttachment(p.state, noclip::parameters::curve, curve)
{
    setLookAndFeel(&look);
    icon = juce::ImageCache::getFromMemory(NoclipArtwork::icon_png, NoclipArtwork::icon_pngSize);
    // Generate once on the message thread. A fixed seed keeps the sand still
    // during meter repaints and resizing; the small tile keeps rendering cheap.
    sandTexture = juce::Image(juce::Image::RGB, 256, 256, false);
    juce::Random grain(0x4e434c50);
    {
        juce::Image::BitmapData pixels(sandTexture, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
            {
                const int variation = grain.nextInt(25) - 12;
                const auto base = noclip::ui::sand;
                pixels.setPixelColour(x, y, juce::Colour(
                    static_cast<juce::uint8>(base.getRed() + variation),
                    static_cast<juce::uint8>(base.getGreen() + variation),
                    static_cast<juce::uint8>(base.getBlue() + variation)));
            }
    }
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        const auto& spec = noclip::parameters::floats[i];
        // Slider maintains explicit textbox colours and copies them into its Label.
        knob.setColour(juce::Slider::textBoxTextColourId, noclip::ui::ink);
        knob.setColour(juce::Slider::textBoxBackgroundColourId, noclip::ui::light);
        knob.setColour(juce::Slider::textBoxOutlineColourId, noclip::ui::edge);
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 116, 26);
        if (juce::String(spec.unit).isNotEmpty()) knob.setTextValueSuffix(" " + juce::String(spec.unit));
        knob.setDoubleClickReturnValue(true, spec.initial);
        knob.setName(spec.name);
        addAndMakeVisible(knob);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, spec.id, knob);
        // This display font maps ASCII '-' to a pictogram. Use a real minus sign;
        // allow both forms when the user types a value. Other text uses the supplied font.
        const int decimals = juce::String(spec.unit).isNotEmpty() ? 2 : 3;
        knob.textFromValueFunction = [decimals](double value)
        {
            return juce::String(value, decimals).replaceCharacter('-', juce::juce_wchar(0x2212));
        };
        knob.valueFromTextFunction = [](const juce::String& text)
        {
            return text.replaceCharacter(juce::juce_wchar(0x2212), '-').getDoubleValue();
        };
        knob.updateText();
        labels[i].setText(juce::String(spec.name).toUpperCase(), juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(labels[i]);
    }
    knobs[0].setTooltip("Input gain affects both wet and dry paths.");
    knobs[1].setTooltip("Output gain before the enabled clipper's sample-peak ceiling.");
    knobs[2].setTooltip("Drive into the nonlinear curve. Dry path does not receive Drive.");
    knobs[3].setTooltip("Amplitude scale of the nonlinear curve; soft curves bend below this level.");
    knobs[4].setTooltip("0 = selected soft curve; 1 = hard clipping. Continuous morph.");
    knobs[5].setTooltip("Transition width for Adjustable knee only. 0 = hard; 1 = broad.");
    knobs[6].setTooltip("Sample-peak clamp after output and mix when CLIP is enabled. Not true peak.");
    knobs[7].setTooltip("0 = dry, 1 = shaped. Output gain and enabled ceiling still apply.");
    curve.addItemList(noclip::parameters::curveNames(), 1);
    curve.setSelectedItemIndex(static_cast<int>(p.state.getRawParameterValue(noclip::parameters::curve)->load()),
                               juce::dontSendNotification);
    curve.setTooltip("Soft clipping family. Curve changes crossfade over 20 ms.");
    addAndMakeVisible(curve); addAndMakeVisible(bypass); addAndMakeVisible(enabled);
    enabled.setTooltip("Enable the nonlinear engine, selected routing and sample ceiling. Off retains input/output gain.");
    addAndMakeVisible(advanced); addAndMakeVisible(sidechain);
    for(auto* button:{&snapshotA,&snapshotB,&copyAB,&copyBA}) { addAndMakeVisible(button); button->setName("compact"); }
    snapshotA.onClick=[this]{processor.selectSnapshot(0);}; snapshotB.onClick=[this]{processor.selectSnapshot(1);};
    copyAB.onClick=[this]{processor.copySnapshot(0,1);}; copyBA.onClick=[this]{processor.copySnapshot(1,0);};
    copyAB.setTooltip("Replace whole-plugin B with A, including curves, quality and sidechain.");
    copyBA.setTooltip("Replace whole-plugin A with B, including curves, quality and sidechain.");
    addAndMakeVisible(match); addAndMakeVisible(compareStatus); compareStatus.setName("compact");
    matchAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"abMatch",match);
    match.setTooltip("Runs A and B on the same input. RMS-based attenuation only, up to 12 dB. Allow a few seconds to settle. Not LUFS matching.");
    quality.addItemList({"1X CREATIVE","2X ECO","4X NORMAL","8X HIGH"},1);
    quality.setTooltip("Shared FIR oversampling. Fixed latency across qualities; changes warm up then crossfade.");
    addAndMakeVisible(quality);
    addAndMakeVisible(clipDc);
    clipDcAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"dcFilter",clipDc);
    clipDc.setTooltip("Optional 10 Hz DC high pass on the wet signal. Also available in SHAPE and 808.");
    qualityAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"quality",quality);
    const juce::StringArray names{"CLIP","SHAPE","808 ENGINE","SIDECHAIN"};
    for(size_t i=0;i<pages.size();++i)
    {
        pages[i].setButtonText(names[static_cast<int>(i)]); addAndMakeVisible(pages[i]);
        pages[i].onClick=[this,i]
        {
            sidechainView=i==3;
            if(sidechainView) { updatePage(); return; }
            processor.setParameter("routing",i==2?1.0f:0.0f);
            processor.setParameter("mode",i==1?1.0f:0.0f); updatePage();
        };
    }
    const auto silence=juce::String(-100.0,1).replaceCharacter('-',juce::juce_wchar(0x2212));
    meterReadouts = {silence,silence,silence,"0"};
    lastMeterTime = juce::Time::getMillisecondCounterHiRes();
    setResizable(true, true);
    setResizeLimits(1000, 820, 1500, 1100);
    setSize(1100, 900);
    updatePage();
    timerCallback();
    startTimerHz(30);
}
NoclipEditor::~NoclipEditor() { stopTimer(); setLookAndFeel(nullptr); }
void NoclipEditor::paint(juce::Graphics& g)
{
    const auto ink=look.ink,sand=look.sand,light=look.light,edge=look.edge,recess=look.recess,
        amber=look.amber,blue=look.blue,green=look.green,glow=look.glow;
    g.setTiledImageFill(sandTexture, 0, 0, 1.0f);
    g.fillRect(getLocalBounds());
    look.bevel(g, getLocalBounds().toFloat().reduced(3));
    g.setColour(edge); g.drawHorizontalLine(106, 20, static_cast<float>(getWidth() - 20));
    g.setColour(light); g.drawHorizontalLine(107, 20, static_cast<float>(getWidth() - 20));

    juce::GlyphArrangement title;
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImageWithin(icon, 24, 24, 72, 72, juce::RectanglePlacement::centred);
    // Preserve the original crosshair's thin upper stroke at small/DPI-scaled
    // sizes. A device-independent vector overdraw prevents raster dropout.
    g.setColour(noclip::ui::ink);
    g.fillRect(59.5f,25.2f,1.25f,11.0f);
    title.addLineOfText(look.font(48), "NOCLIP.DSP", 110, 69);
    juce::Path titlePath;
    title.createPath(titlePath);
    juce::DropShadow(glow.withAlpha(0.9f), 12, {}).drawForPath(g, titlePath);
    juce::DropShadow(green.withAlpha(0.7f), 4, {}).drawForPath(g, titlePath);
    g.setColour(ink); g.fillPath(titlePath);
    g.setColour(ink); g.setFont(look.font(15));
    g.drawText("NONLINEAR AUDIO PROCESSOR", 112, 80, 400, 20, juce::Justification::centredLeft);
    g.setColour(blue);
    g.fillRect(getWidth() - 131, 12, 106, 18);
    g.setColour(ink); g.setFont(look.font(13));
    g.drawText("BUILD 0.5.0", getWidth() - 129, 12, 102, 18, juce::Justification::centred);

    g.setFont(look.font(16)); g.setColour(ink);
    g.drawText(currentPage==0?"TRANSFER MONITOR":"INPUT / OUTPUT WAVEFORMS", 28, 116, 470, 22, juce::Justification::centredLeft);
    g.setFont(look.font(13));
    const auto value = [this](const char* id) { return processor.state.getRawParameterValue(id)->load(); };
    const bool ceilingActive = value("clipEnabled") > 0.5f && value("bypass") < 0.5f;
    const auto caption = ceilingActive ? "CEILING " + juce::String(value("ceilingDb"), 1)
        .replaceCharacter('-', juce::juce_wchar(0x2212)) + " dBFS" : juce::String("CEILING OFF");
    g.drawText("INPUT TO OUTPUT / " + caption, getWidth() - 440, 116, 410, 22, juce::Justification::centredRight);
    const juce::Rectangle<float> area(26, 145, static_cast<float>(getWidth() - 52), 130);
    g.setColour(recess); g.fillRect(area);
    look.bevel(g, area, true);
    for (int i = 1; i < 16; ++i)
    {
        g.setColour(sand.withAlpha(i % 4 == 0 ? 0.16f : 0.06f));
        g.drawVerticalLine(static_cast<int>(area.getX() + area.getWidth() * static_cast<float>(i) / 16.0f),
                           area.getY() + 1, area.getBottom() - 1);
    }
    for (int i = 1; i < 6; ++i)
    {
        g.setColour(sand.withAlpha(i == 3 ? 0.2f : 0.08f));
        g.drawHorizontalLine(static_cast<int>(area.getY() + area.getHeight() * static_cast<float>(i) / 6.0f),
                             area.getX() + 1, area.getRight() - 1);
    }
    noclip::Parameters parameters=processor.readParameters();
    parameters.advanced.dc=false; // Static CLIP preview excludes filter history.
    parameters.inputDb = value("inputDb"); parameters.outputDb = value("outputDb");
    parameters.bypass = value("bypass") > 0.5f; parameters.clipEnabled = value("clipEnabled") > 0.5f;
    parameters.driveDb = value("driveDb"); parameters.thresholdDb = value("thresholdDb");
    parameters.shape = value("shape"); parameters.knee = value("knee");
    parameters.ceilingDb = value("ceilingDb"); parameters.mix = value("mix");
    parameters.curve = static_cast<int>(value("curve"));
    // A separate, settled engine previews the actual signal chain without touching audio state.
    noclip::AudioEngine preview;
    preview.prepare(48000, parameters);
    std::array<float, 257> transfer;
    for (size_t i = 0; i < transfer.size(); ++i) transfer[i] = -2.0f + 4.0f * static_cast<float>(i) / 256.0f;
    float* channel[] { transfer.data() };
    preview.process(channel, 1, static_cast<int>(transfer.size()));
    g.saveState();
    g.reduceClipRegion(area.reduced(2).toNearestInt());
    if (ceilingActive)
    {
        g.setColour(blue.brighter(0.3f));
        const float dashes[] { 6, 4 };
        for (const float sign : { -1.0f, 1.0f })
        {
            const float y = area.getCentreY() + sign * noclip::dbToGain(parameters.ceilingDb) * 50;
            g.drawDashedLine(juce::Line<float>(area.getX(), y, area.getRight(), y), dashes, 2, 1.5f);
        }
    }
    juce::Path path;
    for (int i = 0; i <= 256; ++i)
    {
        const float y = transfer[static_cast<size_t>(i)];
        const float px = area.getX() + 3 + (area.getWidth() - 6) * static_cast<float>(i) / 256.0f;
        const float py = area.getCentreY() - y * 50.0f;
        if (i == 0) path.startNewSubPath(px, py); else path.lineTo(px, py);
    }
    if(currentPage==0)
    {
        g.setColour(amber.withAlpha(0.08f)); g.strokePath(path, juce::PathStrokeType(12));
        g.setColour(amber.withAlpha(0.18f)); g.strokePath(path, juce::PathStrokeType(6));
        g.setColour(amber); g.strokePath(path, juce::PathStrokeType(2));
    }
    else for(int trace=0;trace<2;++trace)
    {
        juce::Path waveform;
        for(size_t i=0;i<256;++i)
        {
            const float y=(trace==0?processor.scopeIn[i]:processor.scopeOut[i]).load();
            const float px=area.getX()+area.getWidth()*static_cast<float>(i)/255;
            const float py=area.getCentreY()-std::clamp(y,-2.0f,2.0f)*50;
            if(i==0) waveform.startNewSubPath(px,py); else waveform.lineTo(px,py);
        }
        g.setColour(trace==0?blue:amber); g.strokePath(waveform,juce::PathStrokeType(1.5f));
    }
    g.restoreState();

    const auto controlArea = getLocalBounds().withTrimmedTop(360).withTrimmedBottom(88).reduced(20, 0);
    const int cellWidth = controlArea.getWidth() / 4, cellHeight = controlArea.getHeight() / 2;
    if(currentPage==0) for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 4; ++col)
        {
            auto cell = juce::Rectangle<int>(controlArea.getX() + col * cellWidth,
                        controlArea.getY() + row * cellHeight, cellWidth, cellHeight).reduced(3);
            look.bevel(g, cell.toFloat());
        }
    auto status = juce::Rectangle<float>(24, static_cast<float>(getHeight() - 76),
                                        static_cast<float>(getWidth() - 48), 34);
    g.setColour(light.withAlpha(0.6f)); g.fillRect(status);
    look.bevel(g, status, true);
    constexpr std::array<const char*, 4> names { "IN", "OUT", "RMS", "CLIP" };
    const int meterWidth = (getWidth() - 48) / 4;
    for (size_t i = 0; i < names.size(); ++i)
    {
        const int x = 24 + static_cast<int>(i) * meterWidth;
        g.setColour(edge);
        if (i != 0) g.drawVerticalLine(x, status.getY() + 5, status.getBottom() - 5);
        g.setColour(ink); g.setFont(look.font(12));
        g.drawText(names[i], x + 8, getHeight() - 74, 50, 30, juce::Justification::centredLeft, false);
        const auto digits = meterReadouts[i].paddedLeft(' ', 6);
        g.setFont(look.font(12).withHorizontalScale(0.65f));
        for (int j = 0; j < 6; ++j)
        {
            const auto digit=digits.substring(j,j+1);
            if(digit==juce::String::charToString(juce::juce_wchar(0x2212)))
                g.fillRect(x+63+j*13,getHeight()-60,7,2);
            else g.drawText(digit,x+60+j*13,getHeight()-74,13,30,juce::Justification::centred,false);
        }
        g.setFont(look.font(10));
        g.drawText(i == 3 ? "%" : "dBFS", x + 145, getHeight() - 74, 40, 30, juce::Justification::centredLeft, false);
    }
    g.setColour(ink); g.setFont(look.font(13));
    const auto footer=juce::String(1<<processor.actualQuality.load())+"X ACTIVE / "+juce::String(processor.getLatencySamples())
        +" SAMPLES LATENCY / SAMPLE PEAK CEILING";
    g.drawText(footer,
               24, getHeight() - 32, getWidth() - 48, 20, juce::Justification::centred);
}
void NoclipEditor::resized()
{
    bypass.setBounds(getWidth() - 142, 40, 116, 28);
    enabled.setBounds(getWidth() - 268, 40, 116, 28);
    curve.setBounds(getWidth() - 268, 76, 242, 26);
    for(size_t i=0;i<pages.size();++i) { pages[i].setName("compact"); pages[i].setBounds(24+static_cast<int>(i)*133,282,125,27); }
    snapshotA.setBounds(24,320,45,27); snapshotB.setBounds(76,320,45,27);
    copyAB.setBounds(137,320,90,27); copyBA.setBounds(235,320,90,27);
    match.setBounds(344,320,160,27); compareStatus.setBounds(520,320,getWidth()-546,27);
    quality.setBounds(getWidth()-270,282,244,27);
    clipDc.setBounds(getWidth()-430,282,150,27);
    auto area = getLocalBounds().withTrimmedTop(360).withTrimmedBottom(88).reduced(20, 0);
    advanced.setBounds(area); sidechain.setBounds(area);
    const int width = area.getWidth() / 4, height = area.getHeight() / 2;
    // Drive, threshold, shape, knee across the top; input, output, ceiling, mix below.
    constexpr std::array<size_t, 8> order { 2, 3, 4, 5, 0, 1, 6, 7 };
    for (size_t pos = 0; pos < order.size(); ++pos)
    {
        auto box = juce::Rectangle<int>(area.getX() + static_cast<int>(pos % 4) * width,
                                       area.getY() + static_cast<int>(pos / 4) * height, width, height);
        box = box.reduced(6);
        labels[order[pos]].setBounds(box.removeFromTop(24));
        knobs[order[pos]].setBounds(box.reduced(6, 0));
    }
}
void NoclipEditor::timerCallback()
{
    updatePage();
    snapshotA.setToggleState(processor.selectedSnapshot()==0,juce::dontSendNotification);
    snapshotB.setToggleState(processor.selectedSnapshot()==1,juce::dontSendNotification);
    compareStatus.setText(processor.comparing.load()?"SWITCHING":(match.getToggleState()?(processor.matchReady.load()?"MATCH TRIM "+juce::String(processor.matchReduction.load(),1)+" dB":"MATCH LEARNING / PLAY AUDIO"):"WHOLE PLUGIN A / B"),juce::dontSendNotification);
    advanced.refresh();
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double elapsed = (now - lastMeterTime) / 1000.0;
    lastMeterTime = now;
    meterBallistics.update(processor.inputPeak.load(), processor.outputPeak.load(),
                           processor.outputRms.load(), processor.clippingActivity.load(), elapsed);
    readoutElapsed += elapsed;
    if (readoutElapsed >= 0.125)
    {
        readoutElapsed = 0;
        const auto readings = meterBallistics.values();
        for (size_t i = 0; i < readings.size(); ++i)
            meterReadouts[i] = juce::String(readings[i], i == 3 ? 0 : 1)
                .replaceCharacter('-', juce::juce_wchar(0x2212));
        repaint(24, getHeight() - 76, getWidth() - 48, 34);
    }
    knobs[5].setEnabled(curve.getSelectedItemIndex() == 3 && knobs[4].getValue() < 1);
    repaint(24, 114, getWidth() - 48, 163);
    repaint(24,getHeight()-32,getWidth()-48,20);
}
void NoclipEditor::updatePage()
{
    const int routing=static_cast<int>(processor.state.getRawParameterValue("routing")->load());
    const int page=sidechainView?3:(routing==0?static_cast<int>(processor.state.getRawParameterValue("mode")->load()):2);
    if(blueTheme!=(routing==1)) applyTheme(routing==1);
    curve.setEnabled(processor.state.getRawParameterValue("mode")->load()<.5f);
    if(page==currentPage) return;
    currentPage=page;
    for(auto& knob:knobs) knob.setVisible(page==0);
    for(auto& label:labels) label.setVisible(page==0);
    advanced.setVisible(page==1||page==2); if(page==1||page==2) advanced.setPage(page);
    sidechain.setVisible(page==3);
    clipDc.setVisible(page==0);
    for(size_t i=0;i<pages.size();++i) pages[i].setToggleState(static_cast<int>(i)==page,juce::dontSendNotification);
    repaint();
}

void NoclipEditor::applyTheme(bool enabledBlue)
{
    blueTheme=enabledBlue; look.setBlueTheme(blueTheme);
    juce::Random grain(0x4e434c50);
    {
        juce::Image::BitmapData pixels(sandTexture,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<256;++y) for(int x=0;x<256;++x)
        {
            const int variation=grain.nextInt(25)-12; const auto base=look.sand;
            pixels.setPixelColour(x,y,juce::Colour(static_cast<juce::uint8>(base.getRed()+variation),static_cast<juce::uint8>(base.getGreen()+variation),static_cast<juce::uint8>(base.getBlue()+variation)));
        }
    }
    const auto update=[this](auto&& self,juce::Component& component)->void
    {
        if(auto* slider=dynamic_cast<juce::Slider*>(&component))
        {
            slider->setColour(juce::Slider::textBoxTextColourId,look.ink);
            slider->setColour(juce::Slider::textBoxBackgroundColourId,look.light);
            slider->setColour(juce::Slider::textBoxOutlineColourId,look.edge);
        }
        for(auto* child:component.getChildren()) self(self,*child);
    };
    update(update,*this); sendLookAndFeelChange(); repaint();
}
