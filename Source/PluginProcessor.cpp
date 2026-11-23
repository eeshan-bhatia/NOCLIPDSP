#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <charconv>

static_assert(std::atomic<float>::is_always_lock_free);
NoclipProcessor::NoclipProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread("noclip curve builder"), state(*this, nullptr, "NOCLIPDSP_STATE", noclip::parameters::createLayout())
{
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = state.getRawParameterValue(noclip::parameters::ids[i]);
    for(size_t i=0;i<extra.size();++i) extra[i]=state.getRawParameterValue(noclip::parameters::extraFloats[i].id);
    for(size_t i=0;i<options.size();++i) options[i]=state.getRawParameterValue(noclip::parameters::options[i]);
    startThread();
}
NoclipProcessor::~NoclipProcessor() { stopThread(-1); }
noclip::CurveTable NoclipProcessor::readCurveTable() const noexcept
{
    noclip::CurvePoints a,b;
    for(size_t i=0;i<9;++i) { a[i]=extra[9+i]->load(); b[i]=extra[18+i]->load(); }
    return noclip::CurveTable::make(a,b);
}
void NoclipProcessor::run()
{
    std::array<float,18> previous; previous.fill(2);
    while(!threadShouldExit())
    {
        std::array<float,18> points;
        for(size_t i=0;i<18;++i) points[i]=extra[9+i]->load();
        if(points!=previous)
        {
            noclip::CurvePoints a,b;
            std::copy_n(points.begin(),9,a.begin()); std::copy_n(points.begin()+9,9,b.begin());
            curveMailbox.publish(noclip::CurveTable::make(a,b)); previous=points;
        }
        wait(10);
    }
}
noclip::Parameters NoclipProcessor::readParameters() const noexcept
{
    const auto get = [this](size_t i) { return values[i]->load(std::memory_order_relaxed); };
    noclip::Parameters p;
    p.inputDb = get(0); p.outputDb = get(1); p.bypass = get(2) > 0.5f;
    p.clipEnabled = get(3) > 0.5f; p.curve = static_cast<int>(get(4));
    p.driveDb = get(5); p.thresholdDb = get(6); p.shape = get(7); p.knee = get(8);
    p.ceilingDb = get(9); p.mix = get(10);
    auto& a=p.advanced;
    a.custom=options[0]->load()>.5f; a.routing=options[1]->load()>.5f?2:0; a.dc=options[3]->load()>.5f;
    a.bias=a.custom?extra[0]->load():0; a.asymmetry=a.custom?extra[1]->load():0; a.morph=extra[2]->load();
    a.sub=extra[3]->load(); a.harmonics=extra[4]->load(); a.bassDrive=extra[5]->load();
    a.punch=extra[6]->load(); a.splitHz=extra[7]->load(); a.bassOutput=extra[8]->load();
    return p;
}
void NoclipProcessor::prepareToPlay(double sampleRate, int)
{
    audioCurve=readCurveTable();
    engine.prepare(sampleRate,getTotalNumOutputChannels(),readParameters(),static_cast<int>(options[2]->load()),audioCurve);
    setLatencySamples(engine.latency());
    actualQuality.store(engine.activeQuality());
}
bool NoclipProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto out = layout.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet() == out;
}
void NoclipProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    processAudio(buffer, midi, false);
}
void NoclipProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    processAudio(buffer, midi, true);
}
void NoclipProcessor::processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hostBypass)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
    auto parameters = readParameters();
    parameters.bypass = parameters.bypass || hostBypass;
    if(curveMailbox.consume(audioCurve))
    {
        bool current=true;
        for(size_t i=0;i<9;++i) current=current&&audioCurve.sourceA[i]==extra[9+i]->load()&&audioCurve.sourceB[i]==extra[18+i]->load();
        if(current) engine.setTable(audioCurve);
    }
    const int points=std::min(256,buffer.getNumSamples());
    for(int i=0;i<points;++i) scopeIn[static_cast<size_t>(i)].store(buffer.getSample(0,i));
    const auto meter = engine.process(buffer.getArrayOfWritePointers(),
                                     getTotalNumOutputChannels(), buffer.getNumSamples(),parameters,static_cast<int>(options[2]->load()));
    for(int i=0;i<points;++i) scopeOut[static_cast<size_t>(i)].store(buffer.getSample(0,i));
    for(int i=points;i<256;++i) { scopeIn[static_cast<size_t>(i)].store(0); scopeOut[static_cast<size_t>(i)].store(0); }
    actualQuality.store(engine.activeQuality());
    inputPeak.store(meter.inputPeak, std::memory_order_relaxed);
    outputPeak.store(meter.outputPeak, std::memory_order_relaxed);
    outputRms.store(meter.outputRms, std::memory_order_relaxed);
    clippingActivity.store(meter.clippingActivity, std::memory_order_relaxed);
}
juce::AudioProcessorParameter* NoclipProcessor::getBypassParameter() const
{
    return state.getParameter(noclip::parameters::bypass);
}
juce::AudioProcessorEditor* NoclipProcessor::createEditor() { return new NoclipEditor(*this); }
void NoclipProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto snapshot = state.copyState();
    snapshot.setProperty("version", 4, nullptr);
    if (auto xml = snapshot.createXml()) copyXmlToBinary(*xml, destination);
}
void NoclipProcessor::setStateInformation(const void* data, int size)
{
    if (data == nullptr || size <= 0 || size > 1024 * 1024) return;
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto restored = juce::ValueTree::fromXml(*xml);
        const int version = static_cast<int>(restored.getProperty("version", 0));
        if (!restored.hasType("NOCLIPDSP_STATE") || (version < 1 || version > 4)) return;
        auto allIds=noclip::parameters::allIds();
        if(version==3) for(const auto& spec:noclip::parameters::legacyBandFloats) allIds.add(spec.id);
        const int expected = version == 1 ? 3 : (version == 2 ? 11 : allIds.size());
        if (restored.getNumChildren() != expected) return;
        for (int index = 0; index < expected; ++index)
        {
            const auto id = allIds[index];
            int count = 0;
            for (auto child : restored)
            {
                if (child.getProperty("id").toString() != id) continue;
                ++count;
                if (!child.hasType("PARAM") || !child.hasProperty("value")) return;
                const auto encoded = child.getProperty("value").toString().toStdString();
                float value = 0;
                const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), value);
                if (parsed.ec != std::errc() || parsed.ptr != encoded.data() + encoded.size()) return;
                const auto* parameter = state.getParameter(id);
                auto range = parameter != nullptr ? parameter->getNormalisableRange() : juce::NormalisableRange<float>{0,1};
                if(parameter==nullptr) for(const auto& spec:noclip::parameters::legacyBandFloats)
                    if(id==spec.id) range=juce::NormalisableRange<float>{spec.min,spec.max};
                if(version==3&&id=="routing") range=juce::NormalisableRange<float>{0,2};
                if (!std::isfinite(value) || value < range.start || value > range.end) return;
                if (parameter!=nullptr && parameter->isDiscrete() && value != std::round(value)) return;
            }
            if (count != 1) return;
        }
        if (version < 3)
        {
            // Old projects retain their gain-only sound regardless of current settings.
            for (int i = expected; i < allIds.size(); ++i)
            {
                const auto id = allIds[i];
                const auto* parameter = state.getParameter(id);
                juce::ValueTree child("PARAM");
                child.setProperty("id", id, nullptr);
                child.setProperty("value", id == noclip::parameters::clipEnabled || id == "quality" || id == "dcFilter" ? 0.0f
                    : parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
                restored.appendChild(child, nullptr);
            }
            restored.setProperty("version", 3, nullptr);
        }
        if(version==3)
        {
            for(int i=restored.getNumChildren()-1;i>=0;--i)
            {
                auto child=restored.getChild(i);
                const auto id=child.getProperty("id").toString();
                if(state.getParameter(id)==nullptr) restored.removeChild(i,nullptr);
                else if(id=="routing") child.setProperty("value",static_cast<float>(child.getProperty("value"))>1.5f?1.0f:0.0f,nullptr);
            }
        }
        restored.setProperty("version",4,nullptr);
        state.replaceState(restored);
    }
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
