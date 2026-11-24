#include "AdvancedPanel.h"

namespace {
juce::String pointId(int slot,int i) { return "curve"+juce::String(slot==0?"A":"B")+juce::String(i); }
float raw(NoclipProcessor& p,const char* id) { return p.state.getRawParameterValue(id)->load(); }
}
void CurveEditor::paint(juce::Graphics& g)
{
    using noclip::ui::TacticalLookAndFeel;
    const auto& theme=static_cast<TacticalLookAndFeel&>(getLookAndFeel());
    const auto recess=theme.recess,sand=theme.sand,amber=theme.amber,blue=theme.blue,glow=theme.glow;
    g.fillAll(recess); const auto area=plot();
    g.setColour(sand.withAlpha(.2f));
    for(int i=0;i<9;++i) g.drawVerticalLine(static_cast<int>(area.getX()+area.getWidth()*static_cast<float>(i)/8),area.getY(),area.getBottom());
    g.drawHorizontalLine(static_cast<int>(area.getCentreY()),area.getX(),area.getRight());
    const int selected=static_cast<int>(raw(processor,"editSlot"));
    const auto tables=processor.readCurveTable();
    for(int bank=0;bank<2;++bank)
    {
        const auto& table=bank==0?tables.a:tables.b;
        juce::Path line;
        for(size_t i=0;i<noclip::CurveTable::size;i+=8)
        {
            const float x=area.getX()+area.getWidth()*static_cast<float>(i)/2048;
            const float y=area.getCentreY()-table[i]*area.getHeight()*.5f;
            if(i==0) line.startNewSubPath(x,y); else line.lineTo(x,y);
        }
        g.setColour(bank==selected?amber:blue.withAlpha(.45f)); g.strokePath(line,juce::PathStrokeType(bank==selected?2.5f:1));
    }
    for(int i=0;i<9;++i)
    {
        const auto id=pointId(selected,i);
        const float y=processor.state.getRawParameterValue(id)->load();
        const float px=area.getX()+area.getWidth()*static_cast<float>(i)/8;
        const float py=area.getCentreY()-y*area.getHeight()*.5f;
        g.setColour(glow.withAlpha(.2f)); g.fillEllipse(px-7,py-7,14,14);
        g.setColour(amber); g.fillEllipse(px-4,py-4,8,8);
    }
}
void CurveEditor::mouseDown(const juce::MouseEvent& e)
{
    slot=static_cast<int>(raw(processor,"editSlot"));
    dragged=std::clamp(static_cast<int>(std::lround((e.position.x-plot().getX())/plot().getWidth()*8)),0,8);
    mirrored=raw(processor,"symmetry")>.5f;
    processor.state.getParameter(pointId(slot,dragged))->beginChangeGesture();
    if(mirrored&&dragged!=4) processor.state.getParameter(pointId(slot,8-dragged))->beginChangeGesture();
    mouseDrag(e);
}
void CurveEditor::mouseDrag(const juce::MouseEvent& e)
{
    if(dragged<0) return;
    float y=std::clamp((plot().getCentreY()-e.position.y)*2/plot().getHeight(),-1.0f,1.0f);
    if(mirrored&&dragged==4) y=0;
    auto* p=processor.state.getParameter(pointId(slot,dragged)); p->setValueNotifyingHost(p->convertTo0to1(y));
    if(mirrored&&dragged!=4)
    { p=processor.state.getParameter(pointId(slot,8-dragged)); p->setValueNotifyingHost(p->convertTo0to1(-y)); }
    repaint();
}
void CurveEditor::mouseUp(const juce::MouseEvent&)
{
    if(dragged<0) return;
    processor.state.getParameter(pointId(slot,dragged))->endChangeGesture();
    if(mirrored&&dragged!=4) processor.state.getParameter(pointId(slot,8-dragged))->endChangeGesture();
    dragged=-1;
}
AdvancedPanel::AdvancedPanel(NoclipProcessor& p) : processor(p),curve(p)
{
    addAndMakeVisible(curve);
    const auto configure=[this,&p](Control& c,const noclip::parameters::FloatSpec& spec)
    {
        c.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        c.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,116,26);
        c.slider.setColour(juce::Slider::textBoxTextColourId,noclip::ui::ink);
        c.slider.setColour(juce::Slider::textBoxBackgroundColourId,noclip::ui::light);
        c.slider.setColour(juce::Slider::textBoxOutlineColourId,noclip::ui::edge);
        c.slider.setName(spec.name); c.label.setName("compact");
        c.slider.setDoubleClickReturnValue(true,spec.initial);
        c.label.setText(juce::String(spec.name).toUpperCase(),juce::dontSendNotification);
        c.label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(c.label); addAndMakeVisible(c.slider);
        c.attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,spec.id,c.slider);
        c.slider.textFromValueFunction=[unit=juce::String(spec.unit)](double v)
        { return juce::String(v,unit=="Hz"?0:2).replaceCharacter('-',juce::juce_wchar(0x2212))+(unit.isEmpty()?juce::String():" "+unit); };
        c.slider.valueFromTextFunction=[](const juce::String& t) { return t.replaceCharacter(juce::juce_wchar(0x2212),'-').getDoubleValue(); };
        c.slider.updateText();
    };
    constexpr std::array<size_t,6> indices{2,3,0,1,6,7};
    for(size_t i=0;i<shared.size();++i) configure(shared[i],noclip::parameters::floats[indices[i]]);
    for(size_t i=0;i<controls.size();++i) configure(controls[i],noclip::parameters::extraFloats[i]);
    controls[3].slider.setTooltip("Blend clean low frequencies back below Split. Harmonics above Split can still change the fundamental near the crossover.");
    controls[4].slider.setTooltip("Blend distorted upper frequencies with the clean upper branch.");
    controls[6].slider.setTooltip("Ease 808 drive during attacks. Never adds gain above the selected drive.");
    controls[7].slider.setTooltip("Boundary between clean sub preservation and distorted upper bass.");
    slot.addItemList({"EDIT A","EDIT B"},1); curvePreset.addItemList({"LINEAR","SOFT","HARD","FOLD"},1);
    curvePreset.setTextWhenNothingSelected("PRESETS");
    for(auto* box:{&slot,&curvePreset,&bassPreset,&nonlinear}) box->setName("compact");
    symmetry.setName("compact"); dc.setName("compact");
    bassPreset.addItemList({"808 DESTROY","SUB CONTROL","HARMONIC BOOST","CLEAN 808","DIGITAL BASS"},1);
    bassPreset.setTextWhenNothingSelected("808 PRESETS");
    nonlinear.addItemList({"CLIP","SHAPE"},1);
    nonlinear.setTooltip("Use the built-in clipper or your saved A/B custom curve in the 808 engine.");
    for(auto* c:std::array<juce::Component*,10>{&slot,&curvePreset,&bassPreset,&symmetry,&dc,&invert,&copyAB,&copyBA,&nonlinear,&gainViewButton}) addAndMakeVisible(c);
    slotAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"editSlot",slot);
    nonlinearAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"mode",nonlinear);
    symmetryAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"symmetry",symmetry);
    dcAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"dcFilter",dc);
    curvePreset.onChange=[this] { if(curvePreset.getSelectedId()>0) applyCurvePreset(curvePreset.getSelectedItemIndex()); curvePreset.setSelectedId(0,juce::dontSendNotification); };
    bassPreset.onChange=[this] { if(bassPreset.getSelectedId()>0) processor.loadBassPreset(bassPreset.getSelectedItemIndex()); bassPreset.setSelectedId(0,juce::dontSendNotification); };
    invert.onClick=[this] { const int bank=slot.getSelectedItemIndex(); for(int i=0;i<9;++i) { const auto id=pointId(bank,i); processor.setParameter(id.toRawUTF8(),-processor.state.getRawParameterValue(id)->load()); } };
    copyAB.onClick=[this] { copyCurve(0,1); }; copyBA.onClick=[this] { copyCurve(1,0); };
    gainViewButton.onClick=[this] { gainView=!gainView; setPage(page); };
    setPage(1);
}
void AdvancedPanel::copyCurve(int from,int to)
{
    for(int i=0;i<9;++i) { const auto id=pointId(to,i); processor.setParameter(id.toRawUTF8(),processor.state.getRawParameterValue(pointId(from,i))->load()); }
}
void AdvancedPanel::applyCurvePreset(int preset)
{
    const int bank=slot.getSelectedItemIndex();
    for(int i=0;i<9;++i)
    {
        const float x=static_cast<float>(i-4)/4;
        float y=x;
        if(preset==1) y=std::tanh(x*2)/std::tanh(2.0f);
        if(preset==2) y=std::clamp(x*3,-1.0f,1.0f);
        if(preset==3) y=std::sin(x*std::numbers::pi_v<float>);
        const auto id=pointId(bank,i); processor.setParameter(id.toRawUTF8(),y);
    }
}
void AdvancedPanel::setPage(int next)
{
    if(page!=next) gainView=false;
    page=next;
    for(size_t i=0;i<controls.size();++i)
    {
        const bool visible=!gainView&&(page==1?i<3:i>=3);
        controls[i].slider.setVisible(visible); controls[i].label.setVisible(visible);
    }
    for(auto& c:shared) { c.slider.setVisible(gainView); c.label.setVisible(gainView); }
    for(auto* c:std::array<juce::Component*,7>{&slot,&curvePreset,&symmetry,&invert,&copyAB,&copyBA,&curve}) c->setVisible(page==1&&!gainView);
    bassPreset.setVisible(page==2&&!gainView); nonlinear.setVisible(page==2&&!gainView);
    gainViewButton.setButtonText(gainView?"BACK":"GAIN / MIX");
    resized(); repaint();
}
void AdvancedPanel::refresh() { repaint(); }
void AdvancedPanel::place(size_t i,juce::Rectangle<int> r)
{
    r=r.reduced(8,4); controls[i].label.setBounds(r.removeFromTop(24)); controls[i].slider.setBounds(r);
}
void AdvancedPanel::resized()
{
    const int w=getWidth(),h=getHeight();
    dc.setBounds(w-145,0,140,28); gainViewButton.setBounds(w-300,0,145,28);
    if(gainView)
    {
        for(size_t i=0;i<shared.size();++i)
        {
            auto cell=juce::Rectangle<int>(static_cast<int>(i%3)*w/3,40+static_cast<int>(i/3)*(h-40)/2,w/3,(h-40)/2).reduced(8,4);
            shared[i].label.setBounds(cell.removeFromTop(24)); shared[i].slider.setBounds(cell);
        }
    }
    else if(page==1)
    {
        slot.setBounds(4,0,100,26); curvePreset.setBounds(110,0,155,26);
        symmetry.setBounds(270,0,140,26); invert.setBounds(416,0,80,26);
        copyAB.setBounds(502,0,74,26); copyBA.setBounds(582,0,74,26);
        const int curveHeight=(h-44)/2;
        curve.setBounds(4,36,w-8,curveHeight);
        for(size_t i=0;i<3;++i) place(i,{static_cast<int>(i)*w/3,40+curveHeight,w/3,h-40-curveHeight});
    }
    else
    {
        bassPreset.setBounds(4,0,270,28); nonlinear.setBounds(285,0,150,28);
        const int cellWidth=std::min(220,w/3),cellHeight=std::min(190,(h-40)/2);
        const int left=(w-3*cellWidth)/2;
        for(size_t i=0;i<6;++i) place(3+i,{left+static_cast<int>(i%3)*cellWidth,40+static_cast<int>(i/3)*cellHeight,cellWidth,cellHeight});
    }
}
void AdvancedPanel::paint(juce::Graphics& g)
{
    g.setColour(static_cast<noclip::ui::TacticalLookAndFeel&>(getLookAndFeel()).edge); g.drawRect(getLocalBounds(),1);
}
