#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <charconv>

static_assert(std::atomic<float>::is_always_lock_free);
NoclipProcessor::NoclipProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread("noclip curve builder"),
      state(*this, nullptr, "NOCLIPDSP_STATE", noclip::parameters::createLayout()),
      engine(std::make_unique<noclip::ComparisonEngine>())
{
    const auto ids = noclip::parameters::allIds();
    for (size_t i = 0; i < raw.size(); ++i)
    {
        raw[i] = state.getRawParameterValue(ids[static_cast<int>(i)]);
        controls[i] = state.getParameter(ids[static_cast<int>(i)]);
    }
    match = state.getRawParameterValue("abMatch");
    store(0, capture(0));
    store(1, capture(0));
    startThread();
}
NoclipProcessor::~NoclipProcessor()
{
    stopThread(-1);
}
NoclipProcessor::Snapshot NoclipProcessor::capture(int slot) const noexcept
{
    Snapshot result;
    const bool live = slot == selected.load();
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = live ? raw[i]->load() : saved[static_cast<size_t>(slot)][i].load();
    return result;
}
void NoclipProcessor::store(int slot, const Snapshot &values) noexcept
{
    for (size_t i = 0; i < values.size(); ++i)
        saved[static_cast<size_t>(slot)][i].store(values[i]);
}
void NoclipProcessor::apply(const Snapshot &values)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        controls[i]->beginChangeGesture();
        controls[i]->setValueNotifyingHost(controls[i]->convertTo0to1(values[i]));
        controls[i]->endChangeGesture();
    }
}
void NoclipProcessor::selectSnapshot(int slot)
{
    slot = std::clamp(slot, 0, 1);
    const juce::ScopedLock lock(snapshotLock);
    if (slot == selected.load())
        return;
    const auto target = capture(slot);
    revision.fetch_add(1);
    store(selected.load(), capture(selected.load()));
    apply(target);
    selected.store(slot);
    revision.fetch_add(1);
}
void NoclipProcessor::copySnapshot(int from, int to)
{
    if (from < 0 || from > 1 || to < 0 || to > 1 || from == to)
        return;
    const juce::ScopedLock lock(snapshotLock);
    const auto source = capture(from);
    revision.fetch_add(1);
    store(to, source);
    if (to == selected.load())
        apply(source);
    revision.fetch_add(1);
}
noclip::CurveTable NoclipProcessor::curveFor(const Snapshot &v, int stage) noexcept
{
    noclip::CurvePoints a, b;
    const size_t base = 44 + static_cast<size_t>(stage) * 30 + 12;
    for (size_t i = 0; i < 9; ++i)
    {
        a[i] = v[base + i];
        b[i] = v[base + 9 + i];
    }
    return noclip::CurveTable::make(a, b);
}
noclip::CurveTable NoclipProcessor::readCurveTable(int stage) const noexcept
{
    return curveFor(capture(selected.load()), stage);
}
void NoclipProcessor::run()
{
    std::array<std::array<float, 18>, 8> previous;
    for (auto &p : previous)
        p.fill(2);
    while (!threadShouldExit())
    {
        const auto before = revision.load();
        if ((before & 1) == 0)
        {
            const std::array<Snapshot, 2> values{capture(0), capture(1)};
            if (before == revision.load())
                for (size_t slot = 0; slot < 2; ++slot)
                    for (int stage = 0; stage < 4; ++stage)
                    {
                        const size_t index = slot * 4 + static_cast<size_t>(stage);
                        std::array<float, 18> points;
                        std::copy_n(values[slot].begin() + 44 + stage * 30 + 12, 18, points.begin());
                        if (points != previous[index])
                        {
                            (*mailboxes)[index].publish(curveFor(values[slot], stage));
                            previous[index] = points;
                        }
                    }
        }
        wait(10);
    }
}
noclip::ComparisonSettings NoclipProcessor::decode(const Snapshot &v) noexcept
{
    noclip::ComparisonSettings result;
    auto &p = result.parameters;
    p.inputDb = v[0];
    p.outputDb = v[1];
    p.bypass = v[2] > .5f;
    p.clipEnabled = v[3] > .5f;
    p.curve = static_cast<int>(v[4]);
    p.driveDb = v[5];
    p.thresholdDb = v[6];
    p.shape = v[7];
    p.knee = v[8];
    p.ceilingDb = v[9];
    p.mix = v[10];
    auto &a = p.advanced;
    a.custom = v[38] > .5f;
    a.routing = v[39] > .5f ? 2 : 0;
    a.dc = v[41] > .5f;
    a.bias = a.custom ? v[11] : 0;
    a.asymmetry = a.custom ? v[12] : 0;
    a.morph = v[13];
    a.sub = v[14];
    a.harmonics = v[15];
    a.bassDrive = v[16];
    a.punch = v[17];
    a.splitHz = v[18];
    a.bassOutput = v[19];
    p.modular.active = true;
    p.modular.lowHz = v[164];
    p.modular.highHz = v[165];
    p.modular.bass = v[166] > .5f;
    for (size_t i = 0; i < 4; ++i)
    {
        const size_t b = 44 + i * 30;
        auto &t = p.modular.stages[i];
        t.enabled = v[b] > .5f;
        t.clip = v[b + 1] > .5f;
        t.custom = v[b + 2] > .5f;
        t.drive = v[b + 3];
        t.threshold = v[b + 4];
        t.hardness = v[b + 5];
        t.knee = v[b + 6];
        t.curve = static_cast<int>(v[b + 7]);
        t.bias = v[b + 8];
        t.asymmetry = v[b + 9];
        t.morph = v[b + 10];
        t.mix = v[b + 11];
    }
    result.quality = static_cast<int>(v[40]);
    return result;
}
noclip::Parameters NoclipProcessor::readParameters() const noexcept
{
    return decode(capture(selected.load())).parameters;
}
void NoclipProcessor::prepareToPlay(double rate, int)
{
    const juce::ScopedLock lock(snapshotLock);
    audioValues = {capture(0), capture(1)};
    audioSelected = selected.load();
    for (size_t i = 0; i < 2; ++i)
        audioSettings[i] = decode(audioValues[i]);
    engine->prepare(rate, getTotalNumOutputChannels(), audioSettings, audioSelected,
                    {curveFor(audioValues[0]), curveFor(audioValues[1])});
    for (int slot = 0; slot < 2; ++slot)
        for (int stage = 0; stage < 4; ++stage)
            engine->setStageTable(slot, stage, curveFor(audioValues[slot], stage), true);
    pending.fill(false);
    setLatencySamples(engine->latency());
    actualQuality.store(engine->activeQuality());
}
bool NoclipProcessor::isBusesLayoutSupported(const BusesLayout &layout) const
{
    const auto out = layout.getMainOutputChannelSet();
    if ((out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) ||
        layout.getMainInputChannelSet() != out)
        return false;
    if (layout.inputBuses.size() != 1 || layout.outputBuses.size() != 1)
        return false;
    return true;
}
void NoclipProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    processAudio(buffer, midi, false);
}
void NoclipProcessor::processBlockBypassed(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    processAudio(buffer, midi, true);
}
void NoclipProcessor::processAudio(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi, bool hostBypass)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const auto before = revision.load();
    if ((before & 1) == 0)
    {
        const std::array<Snapshot, 2> next{capture(0), capture(1)};
        const int slot = selected.load();
        if (before == revision.load())
        {
            audioValues = next;
            audioSelected = slot;
            for (size_t i = 0; i < 2; ++i)
                audioSettings[i] = decode(next[i]);
        }
    }
    for (size_t slot = 0; slot < 2; ++slot)
        for (int stage = 0; stage < 4; ++stage)
        {
            const size_t index = slot * 4 + static_cast<size_t>(stage),
                         base = 44 + static_cast<size_t>(stage) * 30 + 12;
            if ((*mailboxes)[index].consume(pendingTables[index]))
                pending[index] = true;
            const auto &table = pendingTables[index];
            if (pending[index])
            {
                bool valid = true;
                for (size_t i = 0; i < 9; ++i)
                    valid = valid && table.sourceA[i] == audioValues[slot][base + i] &&
                            table.sourceB[i] == audioValues[slot][base + 9 + i];
                if (valid)
                {
                    engine->setStageTable(static_cast<int>(slot), stage, table);
                    pending[index] = false;
                }
            }
        }
    auto main = getBusBuffer(buffer, false, 0);
    const int points = std::min(256, main.getNumSamples());
    for (int i = 0; i < points; ++i)
        scopeIn[static_cast<size_t>(i)].store(main.getSample(0, i));
    const auto meters =
        engine->process(main.getArrayOfWritePointers(), main.getNumChannels(), main.getNumSamples(),
                        audioSettings, audioSelected, match->load() > .5f, hostBypass);
    for (int i = 0; i < points; ++i)
        scopeOut[static_cast<size_t>(i)].store(main.getSample(0, i));
    for (int i = points; i < 256; ++i)
    {
        scopeIn[static_cast<size_t>(i)].store(0);
        scopeOut[static_cast<size_t>(i)].store(0);
    }
    actualQuality.store(engine->activeQuality());
    matchReduction.store(engine->matchAttenuation());
    matchReady.store(engine->ready());
    comparing.store(engine->switching());
    inputPeak.store(meters.inputPeak);
    outputPeak.store(meters.outputPeak);
    outputRms.store(meters.outputRms);
    clippingActivity.store(meters.clippingActivity);
}
juce::AudioProcessorParameter *NoclipProcessor::getBypassParameter() const
{
    return state.getParameter(noclip::parameters::bypass);
}
juce::AudioProcessorEditor *NoclipProcessor::createEditor()
{
    return new NoclipEditor(*this);
}
void NoclipProcessor::getStateInformation(juce::MemoryBlock &destination)
{
    const juce::ScopedLock lock(snapshotLock);
    auto tree = state.copyState();
    tree.setProperty("version", 6, nullptr);
    juce::ValueTree comparison("COMPARE");
    comparison.setProperty("active", selected.load(), nullptr);
    const auto ids = noclip::parameters::allIds();
    for (int slot = 0; slot < 2; ++slot)
    {
        juce::ValueTree child("SLOT");
        child.setProperty("index", slot, nullptr);
        auto values = capture(slot);
        for (size_t i = 0; i < values.size(); ++i)
        {
            juce::ValueTree entry("PARAM");
            const auto id = ids[static_cast<int>(i)];
            entry.setProperty("id", id, nullptr);
            // Use the same APVTS snapshot for the active slot and root state.
            auto root = tree.getChildWithProperty("id", id);
            entry.setProperty(
                "value", slot == selected.load() ? root.getProperty("value") : juce::var(values[i]), nullptr);
            child.appendChild(entry, nullptr);
        }
        comparison.appendChild(child, nullptr);
    }
    tree.appendChild(comparison, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destination);
}
void NoclipProcessor::setStateInformation(const void *data, int size)
{
    if (!data || size <= 0 || size > 1024 * 1024)
        return;
    auto xml = getXmlFromBinary(data, size);
    if (!xml)
        return;
    auto tree = juce::ValueTree::fromXml(*xml);
    const int version = static_cast<int>(tree.getProperty("version", 0));
    if (!tree.hasType("NOCLIPDSP_STATE") || version < 1 || version > 6)
        return;
    auto names = version == 6 ? noclip::parameters::allIds() : noclip::parameters::legacyIds();
    if (version == 1)
        while (names.size() > 3)
            names.remove(names.size() - 1);
    if (version == 2)
        while (names.size() > 11)
            names.remove(names.size() - 1);
    if (version == 3)
        for (auto spec : noclip::parameters::legacyBandFloats)
            names.add(spec.id);
    if (version == 5)
        names.addArray({"scSensitivity", "scAmount", "scAttack", "scRelease", "scEnabled", "abMatch"});
    auto comparison = tree.getChildWithName("COMPARE");
    if (version >= 5)
    {
        if (!comparison.isValid())
            return;
        tree.removeChild(comparison, nullptr);
    }
    const auto valid = [&](const juce::ValueTree &root, const juce::StringArray &ids)
    {
        if (root.getNumChildren() != ids.size())
            return false;
        for (auto id : ids)
        {
            int count = 0;
            for (auto child : root)
                if (child.getProperty("id").toString() == id)
                {
                    ++count;
                    if (!child.hasType("PARAM") || child.getNumChildren() != 0)
                        return false;
                    auto text = child.getProperty("value").toString().toStdString();
                    float v = 0;
                    auto parsed = std::from_chars(text.data(), text.data() + text.size(), v);
                    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() ||
                        !std::isfinite(v))
                        return false;
                    auto *parameter = state.getParameter(id);
                    float lo = 0, hi = 1;
                    if (parameter)
                    {
                        auto r = parameter->getNormalisableRange();
                        lo = r.start;
                        hi = r.end;
                        if (parameter->isDiscrete() && v != std::round(v))
                            return false;
                    }
                    else
                        for (auto spec : noclip::parameters::legacyBandFloats)
                            if (id == spec.id)
                            {
                                lo = spec.min;
                                hi = spec.max;
                            }
                    if (version == 3 && id == "routing")
                        hi = 2;
                    if (id == "scSensitivity")
                    {
                        lo = -60;
                        hi = 0;
                    }
                    if (id == "scAmount")
                        hi = 24;
                    if (id == "scAttack")
                    {
                        lo = .1f;
                        hi = 100;
                    }
                    if (id == "scRelease")
                    {
                        lo = 10;
                        hi = 1000;
                    }
                    if (id == "scEnabled" && v != std::round(v))
                        return false;
                    if (v < lo || v > hi)
                        return false;
                }
            if (count != 1)
                return false;
        }
        return true;
    };
    if (!valid(tree, names))
        return;
    std::array<juce::ValueTree, 2> banks;
    int active = 0;
    if (version >= 5)
    {
        auto encoded = comparison.getProperty("active").toString();
        if (encoded != "0" && encoded != "1")
            return;
        active = encoded.getIntValue();
        if (comparison.getNumChildren() != 2)
            return;
        auto bankNames = names;
        bankNames.removeString("abMatch");
        for (int slot = 0; slot < 2; ++slot)
        {
            int count = 0;
            for (auto child : comparison)
                if (child.getProperty("index").toString() == juce::String(slot))
                {
                    banks[slot] = child.createCopy();
                    ++count;
                }
            if (count != 1 || !banks[slot].hasType("SLOT") || !valid(banks[slot], bankNames))
                return;
            if (slot == active)
                for (auto id : bankNames)
                    if (static_cast<float>(banks[slot].getChildWithProperty("id", id).getProperty("value")) !=
                        static_cast<float>(tree.getChildWithProperty("id", id).getProperty("value")))
                        return;
        }
    }
    else
        banks = {tree.createCopy(), tree.createCopy()};
    const auto all = noclip::parameters::allIds();
    const auto migrate = [&](juce::ValueTree root)
    {
        const auto get = [&](const char *id, float fallback = 0)
        { return static_cast<float>(root.getChildWithProperty("id", id).getProperty("value", fallback)); };
        const float oldRouting = get("routing"), oldMode = get("mode");
        for (int i = root.getNumChildren() - 1; i >= 0; --i)
            if (!all.contains(root.getChild(i).getProperty("id").toString()))
                root.removeChild(i, nullptr);
        for (auto id : all)
            if (!root.getChildWithProperty("id", id).isValid())
            {
                auto *p = state.getParameter(id);
                juce::ValueTree c("PARAM");
                c.setProperty("id", id, nullptr);
                c.setProperty("value",
                              version < 3 && (id == "clipEnabled" || id == "quality" || id == "dcFilter")
                                  ? 0
                                  : p->convertFrom0to1(p->getDefaultValue()),
                              nullptr);
                root.appendChild(c, nullptr);
            }
        const auto put = [&](const juce::String &id, float value)
        { root.getChildWithProperty("id", id).setProperty("value", value, nullptr); };
        if (version < 6)
        {
            const bool bass = version == 3 ? oldRouting > 1.5f : oldRouting > .5f;
            put("routing", bass ? 1.f : 0.f);
            put("masterEnabled", bass ? 0.f : 1.f);
            put("bassEnabled", bass ? 1.f : 0.f);
            put("masterClip", oldMode > .5f ? 0.f : 1.f);
            put("masterCustom", oldMode > .5f ? 1.f : 0.f);
            const char *oldIds[]{"driveDb", "thresholdDb", "shape",     "knee",
                                 "curve",   "bias",        "asymmetry", "curveMorph"};
            const char *newIds[]{"Drive", "Threshold", "Hardness",  "Knee",
                                 "Curve", "Bias",      "Asymmetry", "Morph"};
            for (int i = 0; i < 8; ++i)
                put(noclip::parameters::stageId(0, newIds[i]), get(oldIds[i]));
            for (int slot = 0; slot < 2; ++slot)
                for (int point = 0; point < 9; ++point)
                {
                    auto old = juce::String(slot == 0 ? "curveA" : "curveB") + juce::String(point);
                    put(noclip::parameters::pointId(0, slot, point), get(old.toRawUTF8()));
                }
        }
        return root;
    };
    tree = migrate(tree);
    std::array<Snapshot, 2> snapshots;
    for (int slot = 0; slot < 2; ++slot)
    {
        auto bank = migrate(banks[slot]);
        for (size_t i = 0; i < raw.size(); ++i)
            snapshots[slot][i] = static_cast<float>(
                bank.getChildWithProperty("id", all[static_cast<int>(i)]).getProperty("value"));
    }
    tree.setProperty("version", 6, nullptr);
    const juce::ScopedLock lock(snapshotLock);
    revision.fetch_add(1);
    store(0, snapshots[0]);
    store(1, snapshots[1]);
    state.replaceState(tree);
    selected.store(active);
    revision.fetch_add(1);
}
void NoclipProcessor::setParameter(const char *id, float value)
{
    if (auto *parameter = state.getParameter(id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}
void NoclipProcessor::loadBassPreset(int index)
{
    setParameter("bassEnabled", 1);
    constexpr float settings[5][6]{{.75f, 1, 24, .1f, 100, -6},
                                   {1, .15f, 6, .5f, 140, -2},
                                   {1, 1, 16, .25f, 120, -3},
                                   {1, 0, 0, 0, 120, 0},
                                   {.4f, 1, 30, 0, 90, -8}};
    constexpr const char *names[]{"subPreserve", "harmonics", "bassDrive", "punch", "splitHz", "bassOutput"};
    index = std::clamp(index, 0, 4);
    for (int i = 0; i < 6; ++i)
        setParameter(names[i], settings[index][i]);
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new NoclipProcessor();
}
