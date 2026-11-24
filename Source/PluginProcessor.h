#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ComparisonEngine.h"
#include "Parameters/ParameterLayout.h"
#include <atomic>

class NoclipProcessor final : public juce::AudioProcessor, private juce::Thread
{
public:
    NoclipProcessor();
    ~NoclipProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "noclip.DSP"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState state;
    std::atomic<float> inputPeak { 0 }, outputPeak { 0 }, outputRms { 0 }, clippingActivity { 0 };
    std::atomic<int> actualQuality { 2 };
    noclip::Parameters readParameters() const noexcept;
    noclip::CurveTable readCurveTable() const noexcept;
    void setParameter(const char* id, float value);
    void loadBassPreset(int index);
    void selectSnapshot(int);
    void copySnapshot(int from,int to);
    int selectedSnapshot() const noexcept { return selected.load(); }
    std::atomic<float> sidechainReduction{0}, matchReduction{0};
    std::atomic<bool> sidechainConnected{false}, matchReady{false}, comparing{false};
    // GUI samples actual input/output, not a transfer animation.
    std::array<std::atomic<float>,256> scopeIn {}, scopeOut {};
private:
    void processAudio(juce::AudioBuffer<float>&, juce::MidiBuffer&, bool hostBypass);
    void run() override;
    using Snapshot = std::array<float,noclip::parameters::snapshotCount>;
    std::unique_ptr<noclip::ComparisonEngine> engine;
    std::array<noclip::CurveMailbox,2> mailboxes;
    std::array<noclip::CurveTable,2> pendingTables;
    std::array<bool,2> pending {false,false};
    std::array<std::array<std::atomic<float>,noclip::parameters::snapshotCount>,2> saved {};
    std::array<std::atomic<float>*,noclip::parameters::snapshotCount> raw {};
    std::array<juce::RangedAudioParameter*,noclip::parameters::snapshotCount> controls {};
    std::atomic<float>* match=nullptr;
    std::atomic<int> selected{0};
    std::atomic<unsigned> revision{0};
    juce::CriticalSection snapshotLock;
    std::array<Snapshot,2> audioValues {};
    std::array<noclip::ComparisonSettings,2> audioSettings;
    int audioSelected=0;
    Snapshot capture(int slot) const noexcept;
    void store(int slot,const Snapshot&) noexcept;
    void apply(const Snapshot&);
    static noclip::ComparisonSettings decode(const Snapshot&) noexcept;
    static noclip::CurveTable curveFor(const Snapshot&) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoclipProcessor)
};
