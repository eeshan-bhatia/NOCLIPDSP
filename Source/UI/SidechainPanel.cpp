#include "SidechainPanel.h"
#include "TacticalLookAndFeel.h"
SidechainPanel::SidechainPanel(NoclipProcessor& p):processor(p),enableAttachment(p.state,"scEnabled",enabled)
{
    addAndMakeVisible(enabled); addAndMakeVisible(status); enabled.setName("compact");
    status.setName("compact");
    const char* lines[]{"ROUTE A KICK TO THE PLUGIN SIDECHAIN INPUT.","RELIEF EASES DISTORTION DRIVE; CLEAN SUB IS PRESERVED.","12 dB ABOVE SENSITIVITY GIVES FULL RELIEF. UNITY DRIVE IS THE FLOOR."};
    for(size_t i=0;i<help.size();++i) { help[i].setName("compact"); help[i].setText(lines[i],juce::dontSendNotification); addAndMakeVisible(help[i]); }
    const char* names[]{"SENSITIVITY","DRIVE RELIEF","ATTACK","RELEASE"};
    for(size_t i=0;i<knobs.size();++i)
    {
        auto& k=knobs[i]; const auto spec=noclip::parameters::sidechainFloats[i];
        k.setColour(juce::Slider::textBoxTextColourId,noclip::ui::ink);
        k.setColour(juce::Slider::textBoxBackgroundColourId,noclip::ui::light);
        k.setColour(juce::Slider::textBoxOutlineColourId,noclip::ui::edge);
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,126,26);
        k.setDoubleClickReturnValue(true,spec.initial); k.setName(spec.name);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,spec.id,k);
        k.textFromValueFunction=[unit=juce::String(spec.unit)](double v) { return juce::String(v,1).replaceCharacter('-',juce::juce_wchar(0x2212))+" "+unit; };
        k.valueFromTextFunction=[](const juce::String& t) { return t.replaceCharacter(juce::juce_wchar(0x2212),'-').getDoubleValue(); }; k.updateText();
        labels[i].setText(names[i],juce::dontSendNotification); labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(k); addAndMakeVisible(labels[i]);
    }
    knobs[0].setTooltip("Detector onset in dBFS. Higher key levels progressively ease drive over a 12 dB range.");
    knobs[1].setTooltip("Maximum drive reduction. Cannot turn drive below unity; does not duck the clean sub.");
    timerCallback(); startTimerHz(8);
}
void SidechainPanel::resized()
{
    enabled.setBounds(8,4,250,28); status.setBounds(272,4,getWidth()-280,28);
    const int w=std::min(getWidth(),920),x=(getWidth()-w)/2;
    const int height=std::min(230,getHeight()-128);
    for(size_t i=0;i<4;++i) { auto cell=juce::Rectangle<int>(x+static_cast<int>(i)*w/4,50,w/4,height).reduced(8,0); labels[i].setBounds(cell.removeFromTop(26)); knobs[i].setBounds(cell); }
    for(size_t i=0;i<help.size();++i) help[i].setBounds(20,55+height+static_cast<int>(i)*24,getWidth()-40,22);
}
void SidechainPanel::timerCallback()
{
    juce::String text="SIDECHAIN OFF";
    if(enabled.getToggleState()) text=processor.sidechainConnected.load()?"DETECTOR RELIEF "+juce::String(processor.sidechainReduction.load(),1)+" dB":"NO SIDECHAIN ROUTE";
    status.setText(text,juce::dontSendNotification);
}
