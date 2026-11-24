#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <charconv>

static_assert(std::atomic<float>::is_always_lock_free);
NoclipProcessor::NoclipProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withInput("Sidechain",juce::AudioChannelSet::stereo(),false)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      juce::Thread("noclip curve builder"),state(*this,nullptr,"NOCLIPDSP_STATE",noclip::parameters::createLayout()),
      engine(std::make_unique<noclip::ComparisonEngine>())
{
    const auto ids=noclip::parameters::allIds();
    for(size_t i=0;i<raw.size();++i) { raw[i]=state.getRawParameterValue(ids[static_cast<int>(i)]); controls[i]=state.getParameter(ids[static_cast<int>(i)]); }
    match=state.getRawParameterValue("abMatch");
    store(0,capture(0)); store(1,capture(0)); startThread();
}
NoclipProcessor::~NoclipProcessor() { stopThread(-1); }
NoclipProcessor::Snapshot NoclipProcessor::capture(int slot) const noexcept
{
    Snapshot result; const bool live=slot==selected.load();
    for(size_t i=0;i<result.size();++i) result[i]=live?raw[i]->load():saved[static_cast<size_t>(slot)][i].load();
    return result;
}
void NoclipProcessor::store(int slot,const Snapshot& values) noexcept
{ for(size_t i=0;i<values.size();++i) saved[static_cast<size_t>(slot)][i].store(values[i]); }
void NoclipProcessor::apply(const Snapshot& values)
{ for(size_t i=0;i<values.size();++i) { controls[i]->beginChangeGesture(); controls[i]->setValueNotifyingHost(controls[i]->convertTo0to1(values[i])); controls[i]->endChangeGesture(); } }
void NoclipProcessor::selectSnapshot(int slot)
{
    slot=std::clamp(slot,0,1); const juce::ScopedLock lock(snapshotLock);
    if(slot==selected.load()) return;
    const auto target=capture(slot); revision.fetch_add(1);
    store(selected.load(),capture(selected.load())); apply(target); selected.store(slot); revision.fetch_add(1);
}
void NoclipProcessor::copySnapshot(int from,int to)
{
    if(from<0||from>1||to<0||to>1||from==to) return;
    const juce::ScopedLock lock(snapshotLock); const auto source=capture(from);
    revision.fetch_add(1); store(to,source); if(to==selected.load()) apply(source); revision.fetch_add(1);
}
noclip::CurveTable NoclipProcessor::curveFor(const Snapshot& v) noexcept
{
    noclip::CurvePoints a,b;
    for(size_t i=0;i<9;++i) { a[i]=v[20+i]; b[i]=v[29+i]; }
    return noclip::CurveTable::make(a,b);
}
noclip::CurveTable NoclipProcessor::readCurveTable() const noexcept { return curveFor(capture(selected.load())); }
void NoclipProcessor::run()
{
    std::array<std::array<float,18>,2> previous; for(auto& p:previous) p.fill(2);
    while(!threadShouldExit())
    {
        const auto before=revision.load();
        if((before&1)==0)
        {
            const std::array<Snapshot,2> values{capture(0),capture(1)};
            if(before==revision.load()) for(size_t slot=0;slot<2;++slot)
            {
                std::array<float,18> points; std::copy_n(values[slot].begin()+20,18,points.begin());
                if(points!=previous[slot]) { mailboxes[slot].publish(curveFor(values[slot])); previous[slot]=points; }
            }
        }
        wait(10);
    }
}
noclip::ComparisonSettings NoclipProcessor::decode(const Snapshot& v) noexcept
{
    noclip::ComparisonSettings result; auto& p=result.parameters;
    p.inputDb=v[0]; p.outputDb=v[1]; p.bypass=v[2]>.5f; p.clipEnabled=v[3]>.5f; p.curve=static_cast<int>(v[4]);
    p.driveDb=v[5]; p.thresholdDb=v[6]; p.shape=v[7]; p.knee=v[8]; p.ceilingDb=v[9]; p.mix=v[10];
    auto& a=p.advanced; a.custom=v[38]>.5f; a.routing=v[39]>.5f?2:0; a.dc=v[41]>.5f;
    a.bias=a.custom?v[11]:0; a.asymmetry=a.custom?v[12]:0; a.morph=v[13];
    a.sub=v[14]; a.harmonics=v[15]; a.bassDrive=v[16]; a.punch=v[17]; a.splitHz=v[18]; a.bassOutput=v[19];
    p.sidechain={v[48]>.5f,v[44],v[45],v[46],v[47]}; result.quality=static_cast<int>(v[40]); return result;
}
noclip::Parameters NoclipProcessor::readParameters() const noexcept { return decode(capture(selected.load())).parameters; }
void NoclipProcessor::prepareToPlay(double rate,int)
{
    const juce::ScopedLock lock(snapshotLock);
    audioValues={capture(0),capture(1)}; audioSelected=selected.load();
    for(size_t i=0;i<2;++i) audioSettings[i]=decode(audioValues[i]);
    engine->prepare(rate,getTotalNumOutputChannels(),audioSettings,audioSelected,{curveFor(audioValues[0]),curveFor(audioValues[1])});
    setLatencySamples(engine->latency()); actualQuality.store(engine->activeQuality());
}
bool NoclipProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto out=layout.getMainOutputChannelSet();
    if((out!=juce::AudioChannelSet::mono()&&out!=juce::AudioChannelSet::stereo())||layout.getMainInputChannelSet()!=out) return false;
    if(layout.inputBuses.size()>2||layout.outputBuses.size()!=1) return false;
    if(layout.inputBuses.size()==2) { const auto key=layout.inputBuses[1]; if(!key.isDisabled()&&key!=juce::AudioChannelSet::mono()&&key!=juce::AudioChannelSet::stereo()) return false; }
    return true;
}
void NoclipProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) { processAudio(buffer,midi,false); }
void NoclipProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) { processAudio(buffer,midi,true); }
void NoclipProcessor::processAudio(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi,bool hostBypass)
{
    juce::ScopedNoDenormals noDenormals; midi.clear();
    const auto before=revision.load();
    if((before&1)==0)
    {
        const std::array<Snapshot,2> next{capture(0),capture(1)}; const int slot=selected.load();
        if(before==revision.load()) { audioValues=next; audioSelected=slot; for(size_t i=0;i<2;++i) audioSettings[i]=decode(next[i]); }
    }
    for(size_t slot=0;slot<2;++slot)
    {
        if(mailboxes[slot].consume(pendingTables[slot])) pending[slot]=true;
        const auto& table=pendingTables[slot];
        if(pending[slot])
        {
            bool valid=true;
            for(size_t i=0;i<9;++i) valid=valid&&table.sourceA[i]==audioValues[slot][20+i]&&table.sourceB[i]==audioValues[slot][29+i];
            if(valid) { engine->setTable(static_cast<int>(slot),table); pending[slot]=false; }
        }
    }
    auto main=getBusBuffer(buffer,false,0);
    const bool connected=getBusCount(true)>1&&getBus(true,1)->isEnabled();
    const float* key[2]{}; int keyChannels=0;
    if(connected)
    {
        auto side=getBusBuffer(buffer,true,1); keyChannels=std::min(2,side.getNumChannels());
        for(int ch=0;ch<keyChannels;++ch) key[ch]=side.getReadPointer(ch);
    }
    const int points=std::min(256,main.getNumSamples());
    for(int i=0;i<points;++i) scopeIn[static_cast<size_t>(i)].store(main.getSample(0,i));
    const auto meters=engine->process(main.getArrayOfWritePointers(),main.getNumChannels(),main.getNumSamples(),audioSettings,audioSelected,match->load()>.5f,hostBypass,keyChannels?key:nullptr,keyChannels);
    for(int i=0;i<points;++i) scopeOut[static_cast<size_t>(i)].store(main.getSample(0,i));
    for(int i=points;i<256;++i) { scopeIn[static_cast<size_t>(i)].store(0); scopeOut[static_cast<size_t>(i)].store(0); }
    actualQuality.store(engine->activeQuality()); sidechainConnected.store(connected);
    sidechainReduction.store(engine->sidechainReduction()); matchReduction.store(engine->matchAttenuation());
    matchReady.store(engine->ready()); comparing.store(engine->switching());
    inputPeak.store(meters.inputPeak); outputPeak.store(meters.outputPeak); outputRms.store(meters.outputRms); clippingActivity.store(meters.clippingActivity);
}
juce::AudioProcessorParameter* NoclipProcessor::getBypassParameter() const { return state.getParameter(noclip::parameters::bypass); }
juce::AudioProcessorEditor* NoclipProcessor::createEditor() { return new NoclipEditor(*this); }
void NoclipProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    const juce::ScopedLock lock(snapshotLock); auto tree=state.copyState(); tree.setProperty("version",5,nullptr);
    juce::ValueTree comparison("COMPARE"); comparison.setProperty("active",selected.load(),nullptr);
    const auto ids=noclip::parameters::allIds();
    for(int slot=0;slot<2;++slot)
    {
        juce::ValueTree child("SLOT"); child.setProperty("index",slot,nullptr); auto values=capture(slot);
        for(size_t i=0;i<values.size();++i)
        {
            juce::ValueTree entry("PARAM"); const auto id=ids[static_cast<int>(i)]; entry.setProperty("id",id,nullptr);
            // Use the same APVTS snapshot for the active slot and root state.
            auto root=tree.getChildWithProperty("id",id);
            entry.setProperty("value",slot==selected.load()?root.getProperty("value"):juce::var(values[i]),nullptr); child.appendChild(entry,nullptr);
        }
        comparison.appendChild(child,nullptr);
    }
    tree.appendChild(comparison,nullptr); if(auto xml=tree.createXml()) copyXmlToBinary(*xml,destination);
}
void NoclipProcessor::setStateInformation(const void* data,int size)
{
    if(!data||size<=0||size>1024*1024) return;
    auto xml=getXmlFromBinary(data,size); if(!xml) return;
    auto tree=juce::ValueTree::fromXml(*xml); const int version=static_cast<int>(tree.getProperty("version",0));
    if(!tree.hasType("NOCLIPDSP_STATE")||version<1||version>5) return;
    auto ids=version<5?noclip::parameters::legacyIds():noclip::parameters::allIds();
    if(version==1) while(ids.size()>3) ids.remove(ids.size()-1);
    if(version==2) while(ids.size()>11) ids.remove(ids.size()-1);
    if(version==3) for(auto spec:noclip::parameters::legacyBandFloats) ids.add(spec.id);
    auto comparison=tree.getChildWithName("COMPARE"); if(version==5) { if(!comparison.isValid()) return; tree.removeChild(comparison,nullptr); }
    const auto valid=[&](const juce::ValueTree& root,const juce::StringArray& names,bool legacy)
    {
        if(root.getNumChildren()!=names.size()) return false;
        for(auto id:names)
        {
            int count=0;
            for(auto child:root) if(child.getProperty("id").toString()==id)
            {
                ++count; if(!child.hasType("PARAM")||child.getNumChildren()!=0) return false;
                const auto text=child.getProperty("value").toString().toStdString(); float v=0;
                auto parsed=std::from_chars(text.data(),text.data()+text.size(),v);
                if(parsed.ec!=std::errc()||parsed.ptr!=text.data()+text.size()||!std::isfinite(v)) return false;
                auto* parameter=state.getParameter(id); float lo=0,hi=1;
                if(parameter) { auto range=parameter->getNormalisableRange(); lo=range.start; hi=range.end; if(parameter->isDiscrete()&&v!=std::round(v)) return false; }
                else for(auto spec:noclip::parameters::legacyBandFloats) if(id==spec.id) { lo=spec.min; hi=spec.max; }
                if(legacy&&id=="routing") hi=2;
                if(v<lo||v>hi) return false;
            }
            if(count!=1) return false;
        }
        return true;
    };
    if(!valid(tree,ids,version==3)) return;
    if(version==3) for(int i=tree.getNumChildren()-1;i>=0;--i)
    {
        auto child=tree.getChild(i); auto id=child.getProperty("id").toString();
        if(!state.getParameter(id)) tree.removeChild(i,nullptr);
        else if(id=="routing") child.setProperty("value",static_cast<float>(child.getProperty("value"))>1.5f?1.0f:0.0f,nullptr);
    }
    const auto all=noclip::parameters::allIds();
    for(auto id:all) if(!tree.getChildWithProperty("id",id).isValid())
    {
        auto* p=state.getParameter(id); juce::ValueTree child("PARAM"); child.setProperty("id",id,nullptr);
        const float value=version<3&&(id=="clipEnabled"||id=="quality"||id=="dcFilter")?0:p->convertFrom0to1(p->getDefaultValue());
        child.setProperty("value",value,nullptr); tree.appendChild(child,nullptr);
    }
    std::array<Snapshot,2> snapshots; int active=0;
    if(version==5)
    {
        const auto encoded=comparison.getProperty("active").toString(); if(encoded!="0"&&encoded!="1") return; active=encoded.getIntValue();
        if(comparison.getNumChildren()!=2) return;
        auto names=all; names.remove(names.size()-1);
        for(int slot=0;slot<2;++slot)
        {
            juce::ValueTree bank; int count=0;
            for(auto child:comparison) if(child.getProperty("index").toString()==juce::String(slot)) { bank=child; ++count; }
            if(count!=1||!bank.hasType("SLOT")||!valid(bank,names,false)) return;
            for(size_t i=0;i<raw.size();++i)
            {
                const auto id=all[static_cast<int>(i)]; const float value=static_cast<float>(bank.getChildWithProperty("id",id).getProperty("value")); snapshots[slot][i]=value;
                if(slot==active&&value!=static_cast<float>(tree.getChildWithProperty("id",id).getProperty("value"))) return;
            }
        }
    }
    else for(size_t i=0;i<raw.size();++i) snapshots[0][i]=snapshots[1][i]=static_cast<float>(tree.getChildWithProperty("id",all[static_cast<int>(i)]).getProperty("value"));
    tree.setProperty("version",5,nullptr); const juce::ScopedLock lock(snapshotLock);
    revision.fetch_add(1); store(0,snapshots[0]); store(1,snapshots[1]); state.replaceState(tree); selected.store(active); revision.fetch_add(1);
}
void NoclipProcessor::setParameter(const char* id,float value)
{
    if(auto* parameter=state.getParameter(id))
    {
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); parameter->endChangeGesture();
    }
}
void NoclipProcessor::loadBassPreset(int index)
{
    // Deliberate complete processing starts; never hidden loudness normalisation.
    setParameter("bypass",0); setParameter("clipEnabled",1); setParameter("routing",1); setParameter("mode",0);
    setParameter("inputDb",0); setParameter("outputDb",0); setParameter("driveDb",0); setParameter("thresholdDb",0);
    setParameter("shape",0); setParameter("knee",.2f); setParameter("curve",3); setParameter("mix",1);
    setParameter("bias",0); setParameter("asymmetry",0); setParameter("dcFilter",1); setParameter("quality",2);
    setParameter("ceilingDb",-1);
    constexpr float settings[5][6] {
        { .75f,1,24,.1f,100,-6 }, { 1,.15f,6,.5f,140,-2 },
        { 1,1,16,.25f,120,-3 }, { 1,0,0,0,120,0 }, { .4f,1,30,0,90,-8 }
    };
    constexpr const char* names[] { "subPreserve","harmonics","bassDrive","punch","splitHz","bassOutput" };
    index=std::clamp(index,0,4);
    for(int i=0;i<6;++i) setParameter(names[i],settings[index][i]);
    if(index==4) setParameter("shape",1);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NoclipProcessor(); }
