#include "PluginProcessor.h"
#include <iostream>
#include <limits>
int comparisonTests()
{
    int failures=0;
    const auto check=[&](bool ok,const char* message) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<message<<'\n'; } };
    noclip::SidechainDetector detector; detector.prepare(48000); detector.setParameters({true,-24,12,1,100});
    float relief=1;
    for(int i=0;i<48000;++i) relief=detector.process(1,true);
    check(std::abs(relief-noclip::dbToGain(-12))<1e-5f,"sidechain reaches configured maximum relief");
    for(int i=0;i<48000;++i) relief=detector.process(0,false);
    check(std::abs(relief-1)<1e-5f,"disconnected key returns to unity");
    detector.setParameters({true,-24,0,.1f,10});
    for(int i=0;i<48000;++i) relief=detector.process(1,true);
    check(relief==1,"zero amount neutral");
    check(std::isfinite(detector.process(std::numeric_limits<float>::quiet_NaN(),true)),"invalid key contained");
    noclip::SidechainDetector fast,slow;
    fast.prepare(48000); slow.prepare(48000); fast.setParameters({true,-24,12,.1f,10}); slow.setParameters({true,-24,12,100,1000});
    for(int i=0;i<240;++i) { fast.process(1,true); slow.process(1,true); }
    check(fast.reductionDb()>slow.reductionDb()+2,"attack knob changes onset response");
    for(int i=0;i<48000;++i) { fast.process(1,true); slow.process(1,true); }
    for(int i=0;i<12000;++i) { fast.process(0,true); slow.process(0,true); }
    check(fast.reductionDb()<slow.reductionDb()-2,"release knob changes recovery response");
    noclip::RmsMatcher matcher; matcher.prepare(48000);
    std::array<float,2> gains;
    for(int i=0;i<480000;++i) gains=matcher.next(1,.0001,true,true);
    check(std::abs(gains[0]-noclip::dbToGain(-12))<1e-4f&&gains[1]==1,"match attenuation capped at 12 dB");
    for(int i=0;i<480000;++i) gains=matcher.next(0,0,true,true);
    check(std::isfinite(gains[0])&&std::isfinite(gains[1]),"matching silence remains finite");
    auto engine=std::make_unique<noclip::ComparisonEngine>();
    std::array<noclip::ComparisonSettings,2> settings;
    settings[0].quality=settings[1].quality=0; settings[1].parameters.outputDb=-6;
    const auto curve=noclip::CurveTable::make(); engine->prepare(48000,2,settings,0,{curve,curve});
    std::array<float,256> l,r,key; key.fill(1); float* channels[]{l.data(),r.data()}; const float* keys[]{key.data()};
    float aRms=0,bRms=0; float previous=0,maxJump=0;
    for(int block=0;block<2000;++block)
    {
        l.fill(.2f); r.fill(.2f);
        auto m=engine->process(channels,2,256,settings,block<1700?0:1,true,false);
        if(block==1699) aRms=m.outputRms; if(block==1999) bRms=m.outputRms;
        if(block>10) for(float x:l) { maxJump=std::max(maxJump,std::abs(x-previous)); previous=x; }
        else previous=l.back();
    }
    check(engine->ready()&&std::abs(aRms-bRms)<.0001f,"A/B RMS matching equalizes 6 dB difference");
    check(aRms<=.2f&&engine->matchAttenuation()>=0,"match never boosts");
    check(maxJump<.001f,"matched snapshot transition avoids steps");
    settings[0].quality=3; settings[0].parameters.outputDb=-12;
    engine->prepare(48000,1,settings,0,{curve,curve});
    for(int block=0;block<30;++block) { l.fill(0); engine->process(channels,1,256,settings,0,true,true); }
    l.fill(0); l[0]=.3f;
    engine->process(channels,1,256,settings,0,true,true);
    for(int i=0;i<256;++i) check(l[i]==(i==engine->latency()?.3f:0),"comparison host bypass is exact delay");
    settings[0].quality=0; settings[0].parameters.outputDb=0;
    settings[0].parameters.clipEnabled=true; settings[0].parameters.driveDb=24;
    settings[0].parameters.sidechain={true,-24,24,.1f,10}; settings[1]=settings[0];
    engine->prepare(48000,2,settings,0,{curve,curve});
    for(int block=0;block<200;++block) { l.fill(0);r.fill(0); engine->process(channels,2,256,settings,0,false,false,keys,1); }
    check(l.back()==0&&r.back()==0,"key never leaks into audio");
    for(int block=0;block<200;++block) { l.fill(.02f);r.fill(.02f); engine->process(channels,2,256,settings,0,false,false,keys,1); }
    const float relieved=l.back();
    for(int block=0;block<200;++block) { l.fill(.02f);r.fill(.02f); engine->process(channels,2,256,settings,0,false,false); }
    check(l.back()>relieved*3,"external key eases nonlinear drive");
    // With both bass branches clean, sidechain must make no audible difference.
    settings[0].parameters.advanced.routing=2; settings[0].parameters.advanced.sub=1; settings[0].parameters.advanced.harmonics=0;
    settings[1]=settings[0]; settings[1].parameters.sidechain.enabled=false;
    engine->prepare(48000,2,settings,0,{curve,curve});
    for(int block=0;block<600;++block) { l.fill(.02f);r.fill(.02f); engine->process(channels,2,256,settings,block<300?0:1,false,false,keys,1); if(block==299) aRms=l.back(); }
    check(std::abs(aRms-l.back())<1e-5f,"clean 808 branches preserved under sidechain");
    NoclipProcessor p,q;
    p.setParameter("driveDb",18); p.setParameter("curveA5",.7f); p.setParameter("scEnabled",1); p.setParameter("scAmount",13);
    p.copySnapshot(0,1); p.selectSnapshot(1); p.setParameter("driveDb",4); p.setParameter("curveA5",.3f); p.setParameter("quality",0); p.setParameter("abMatch",1);
    juce::MemoryBlock state; p.getStateInformation(state); q.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    check(q.selectedSnapshot()==1&&q.state.getRawParameterValue("driveDb")->load()==4,"active B recalled");
    q.selectSnapshot(0); check(q.state.getRawParameterValue("driveDb")->load()==18&&std::abs(q.state.getRawParameterValue("curveA5")->load()-.7f)<.001f,"inactive A and curve recalled");
    check(q.state.getRawParameterValue("scAmount")->load()==13&&q.state.getRawParameterValue("abMatch")->load()==1,"sidechain stored; matching global");
    q.copySnapshot(1,0); check(q.state.getRawParameterValue("driveDb")->load()==4,"copy into active slot");
    auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize())); auto tree=juce::ValueTree::fromXml(*xml);
    tree.getChildWithName("COMPARE").getChild(0).getChild(0).setProperty("value",99999,nullptr);
    juce::MemoryBlock invalid,before,after; xml=tree.createXml(); juce::AudioProcessor::copyXmlToBinary(*xml,invalid); q.getStateInformation(before);
    q.setStateInformation(invalid.getData(),static_cast<int>(invalid.getSize())); q.getStateInformation(after); check(before==after,"malformed inactive snapshot rejected atomically");
    auto legacy=p.state.copyState(); legacy.setProperty("version",4,nullptr);
    for(int i=legacy.getNumChildren()-1;i>=0;--i) if(!noclip::parameters::legacyIds().contains(legacy.getChild(i).getProperty("id").toString())) legacy.removeChild(i,nullptr);
    xml=legacy.createXml(); juce::AudioProcessor::copyXmlToBinary(*xml,state); q.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    check(q.selectedSnapshot()==0&&q.state.getRawParameterValue("abMatch")->load()==0&&q.state.getRawParameterValue("scEnabled")->load()==0,"schema 4 migrates with new features off");
    q.selectSnapshot(1); check(q.state.getRawParameterValue("driveDb")->load()==4,"legacy initializes both snapshots");
    auto layout=q.getBusesLayout(); layout.inputBuses.set(1,juce::AudioChannelSet::mono());
    check(q.setBusesLayout(layout),"mono sidechain accepted"); q.setParameter("scEnabled",1); q.prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(3,128); juce::MidiBuffer midi;
    for(int block=0;block<100;++block) { audio.clear(); for(int i=0;i<128;++i) audio.setSample(2,i,1); q.processBlock(audio,midi); }
    check(audio.getMagnitude(0,0,128)==0&&audio.getMagnitude(1,0,128)==0&&audio.getMagnitude(2,0,128)==1,"host sidechain bus separate and unmodified");
    check(q.sidechainConnected.load()&&q.sidechainReduction.load()>5,"host key reaches detector");
    layout.inputBuses.set(1,juce::AudioChannelSet::stereo()); check(q.isBusesLayoutSupported(layout),"stereo sidechain accepted");
    layout.inputBuses.set(1,juce::AudioChannelSet::create5point1()); check(!q.isBusesLayoutSupported(layout),"unsupported key layout rejected");
    // Render editors offscreen; no user windows or audio devices are opened.
    for(int bass=0;bass<3;++bass)
    {
        q.setParameter("routing",bass==1?1.0f:0.0f); q.setParameter("mode",0);
        std::unique_ptr<juce::AudioProcessorEditor> editor(q.createEditor()); editor->setSize(1000,820);
        if(bass==2) for(auto* component:editor->getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(component))
            if(button->getButtonText()=="SIDECHAIN") button->onClick();
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        auto file=juce::File::getCurrentWorkingDirectory().getChildFile(bass==2?"sidechain-preview.png":(bass==1?"808-blue-preview.png":"comparison-preview.png"));
        auto output=file.createOutputStream(); if(output) { output->setPosition(0); output->truncate(); juce::PNGImageFormat png; png.writeImageToStream(image,*output); }
    }
    std::cout<<"Sidechain/comparison failures: "<<failures<<'\n'; return failures;
}
