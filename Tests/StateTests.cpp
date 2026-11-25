#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
int processingTests();
int comparisonTests();

namespace {
void set(NoclipProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.state.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
bool equal(const NoclipProcessor& a, const NoclipProcessor& b)
{
    for (auto id : noclip::parameters::allIds())
        if (std::abs(a.state.getRawParameterValue(id)->load() - b.state.getRawParameterValue(id)->load()) > 0.001f)
        {
            std::cerr << "State mismatch: " << id << " expected " << a.state.getRawParameterValue(id)->load()
                      << " actual " << b.state.getRawParameterValue(id)->load() << '\n';
            return false;
        }
    return true;
}
}
int main()
{
    const auto failure = [](int code) { std::cerr << "StateTests failed at check " << code << '\n'; return code; };
    juce::ScopedJuceInitialiser_GUI gui;
    NoclipProcessor original, restored;
    set(original, noclip::parameters::input, -12.0f);
    set(original, noclip::parameters::output, 3.0f);
    set(original, noclip::parameters::bypass, 1.0f);
    set(original, noclip::parameters::clipEnabled, 0);
    set(original, "driveDb", 18);
    set(original, "thresholdDb", -9);
    set(original, "shape", 0.75f);
    set(original, "knee", 0.6f);
    set(original, "ceilingDb", -3);
    set(original, "mix", 0.4f);
    set(original, "curve", 4);
    set(original,"quality",0); set(original,"dcFilter",0);
    set(original,"curveA5",.7f); set(original,"curveB2",-.9f);
    set(original,"harmonics",.9f); set(original,"subPreserve",.8f);
    juce::MemoryBlock bytes;
    original.getStateInformation(bytes);
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    if (!equal(original, restored)) return failure(1);
    restored.setStateInformation("invalid", 7);
    restored.setStateInformation(nullptr, 10);
    if (!equal(original, restored)) return failure(2);

    auto tree = original.state.copyState();
    tree.setProperty("version", 99, nullptr);
    auto xml = tree.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    if (!equal(original, restored)) return failure(3);
    tree.setProperty("version", 4, nullptr);
    tree.getChild(0).setProperty("value", 9999.0f, nullptr);
    xml = tree.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    if (!equal(original, restored)) return failure(4);

    for (auto channels : { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() })
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(channels);
        layout.outputBuses.add(channels);
        if (!restored.isBusesLayoutSupported(layout)) return failure(5);
    }
    restored.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    audio.clear();
    restored.processBlock(audio, midi);
    if (audio.getMagnitude(0, 128) != 0 || restored.getLatencySamples() <= 0) return failure(6);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i) audio.setSample(ch, i, 0.25f);
    restored.processBlock(audio, midi);
    if (audio.getSample(0, restored.getLatencySamples()) != 0.25f) return failure(7);
    set(restored, noclip::parameters::bypass, 0);
    restored.prepareToPlay(48000, 128);
    for(int ch=0;ch<2;++ch) for(int i=0;i<128;++i) audio.setSample(ch,i,.25f);
    restored.processBlock(audio, midi);
    if (std::abs(audio.getSample(0, restored.getLatencySamples()) - 0.25f * noclip::dbToGain(-9.0f)) > 0.00001f) return failure(8);
    // Schema 1 must migrate to gain-only, resetting all new controls to defaults.
    juce::ValueTree legacy("NOCLIPDSP_STATE");
    legacy.setProperty("version", 1, nullptr);
    for (auto id : { noclip::parameters::input, noclip::parameters::output, noclip::parameters::bypass })
    {
        juce::ValueTree child("PARAM");
        child.setProperty("id", id, nullptr);
        child.setProperty("value", 0.0f, nullptr);
        legacy.appendChild(child, nullptr);
    }
    xml = legacy.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    if (restored.state.getRawParameterValue("clipEnabled")->load() != 0
        || restored.state.getRawParameterValue("driveDb")->load() != 0
        || restored.state.getRawParameterValue("mix")->load() != 1) return failure(9);

    auto invalid = restored.state.copyState();
    invalid.setProperty("version", 4, nullptr);
    invalid.getChild(0).setProperty("value", "garbage", nullptr);
    juce::MemoryBlock before;
    restored.getStateInformation(before);
    xml = invalid.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    juce::MemoryBlock after;
    restored.getStateInformation(after);
    if (before != after) return failure(10);

    // Exercise numeric parsing in an otherwise valid schema, so rejection
    // cannot pass merely because unrelated parameter IDs are invalid.
    for (const char* malformed : { "", "garbage", " 1", "1 ", "+1", "1junk",
                                  "1,5", "0x1", "nan", "inf", "1e999", "1e" })
    {
        auto numeric = legacy.createCopy();
        numeric.getChild(0).setProperty("value", malformed, nullptr);
        xml = numeric.createXml();
        juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
        restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
        restored.getStateInformation(after);
        if (before != after) { std::cerr << "Accepted malformed number: " << malformed << '\n'; return failure(15); }
    }
    auto numeric = legacy.createCopy();
    numeric.getChild(0).setProperty("value", "-1.25e1", nullptr);
    xml = numeric.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    const auto recalledDecimal = restored.state.getRawParameterValue(noclip::parameters::input)->load();
    std::cout << "Scientific-notation recall error: " << std::abs(recalledDecimal + 12.5f) << '\n';
    // Host parameter normalization can round by a few ULPs on different CPUs.
    if (std::abs(recalledDecimal + 12.5f) > 0.0001f) return failure(16);

    set(original, noclip::parameters::output, 24.0f);
    original.getStateInformation(bytes);
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    if (std::abs(restored.state.getRawParameterValue(noclip::parameters::output)->load() - 24.0f) > 0.001f)
        return failure(11);

    auto phase2=original.state.copyState();
    for(int i=phase2.getNumChildren()-1;i>=0;--i)
    {
        const auto id=phase2.getChild(i).getProperty("id").toString();
        bool old=false; for(auto key:noclip::parameters::ids) old=old||id==key;
        if(!old) phase2.removeChild(i,nullptr);
    }
    phase2.setProperty("version",2,nullptr); xml=phase2.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml,bytes);
    restored.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));
    if(restored.state.getRawParameterValue("quality")->load()!=0||restored.state.getRawParameterValue("dcFilter")->load()!=0
        ||restored.state.getRawParameterValue("routing")->load()!=0||std::abs(restored.state.getRawParameterValue("curveA5")->load()-.25f)>.0001f)
    {
        std::cerr << "Legacy migration: quality=" << restored.state.getRawParameterValue("quality")->load()
                  << ", dc=" << restored.state.getRawParameterValue("dcFilter")->load()
                  << ", routing=" << restored.state.getRawParameterValue("routing")->load()
                  << ", curve=" << restored.state.getRawParameterValue("curveA5")->load() << '\n';
        return failure(12);
    }

    auto development=original.state.copyState(); development.setProperty("version",3,nullptr);
    for(int i=development.getNumChildren()-1;i>=0;--i) if(!noclip::parameters::legacyIds().contains(development.getChild(i).getProperty("id").toString())) development.removeChild(i,nullptr);
    for(auto spec:noclip::parameters::legacyBandFloats)
    {
        juce::ValueTree child("PARAM"); child.setProperty("id",spec.id,nullptr); child.setProperty("value",spec.initial,nullptr);
        development.appendChild(child,nullptr);
    }
    for(auto child:development) if(child.getProperty("id").toString()=="routing") child.setProperty("value",2,nullptr);
    xml=development.createXml(); juce::AudioProcessor::copyXmlToBinary(*xml,bytes);
    restored.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));
    if(restored.state.getRawParameterValue("routing")->load()!=1||std::abs(restored.state.getRawParameterValue("curveA5")->load()-.7f)>.001f)
    { std::cerr<<"Development migration: routing="<<restored.state.getRawParameterValue("routing")->load()<<", curve="<<restored.state.getRawParameterValue("curveA5")->load()<<'\n'; return failure(14); }

    std::cout << "State, rejection, layouts, restored audio and latency passed\n";
    return processingTests()+comparisonTests()==0?0:13;
}
