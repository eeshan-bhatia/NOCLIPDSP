#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
int comparisonTests()
{
    int failures = 0;
    const auto check = [&](bool ok, const char *name)
    {
        if (!ok)
        {
            ++failures;
            std::cerr << "FAIL: " << name << '\n';
        }
    };
    noclip::RmsMatcher matcher;
    matcher.prepare(48000);
    std::array<float, 2> gains;
    for (int i = 0; i < 480000; ++i)
        gains = matcher.next(1, .0001, true, true);
    check(std::abs(gains[0] - noclip::dbToGain(-12)) < 1e-4f && gains[1] == 1,
          "match attenuation capped without boosts");
    auto engine = std::make_unique<noclip::ComparisonEngine>();
    std::array<noclip::ComparisonSettings, 2> settings;
    settings[0].quality = settings[1].quality = 0;
    settings[1].parameters.outputDb = -6;
    const auto curve = noclip::CurveTable::make();
    engine->prepare(48000, 2, settings, 0, {curve, curve});
    std::array<float, 256> l, r;
    float *channels[]{l.data(), r.data()};
    float aRms = 0, bRms = 0;
    for (int block = 0; block < 2000; ++block)
    {
        l.fill(.2f);
        r.fill(.2f);
        auto m = engine->process(channels, 2, 256, settings, block < 1700 ? 0 : 1, true, false);
        if (block == 1699)
            aRms = m.outputRms;
        if (block == 1999)
            bRms = m.outputRms;
    }
    check(engine->ready() && std::abs(aRms - bRms) < .0001f, "A/B RMS match");
    auto p = std::make_unique<NoclipProcessor>(), q = std::make_unique<NoclipProcessor>();
    p->setParameter("lowEnabled", 1);
    p->setParameter("lowDrive", 18);
    p->setParameter("lowA5", .7f);
    p->setParameter("highCustom", 1);
    p->setParameter("bassEnabled", 1);
    p->copySnapshot(0, 1);
    p->selectSnapshot(1);
    p->setParameter("lowDrive", 4);
    p->setParameter("lowA5", .3f);
    p->setParameter("abMatch", 1);
    juce::MemoryBlock state;
    p->getStateInformation(state);
    q->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(q->selectedSnapshot() == 1 && q->state.getRawParameterValue("lowDrive")->load() == 4,
          "active B recalled");
    q->selectSnapshot(0);
    check(q->state.getRawParameterValue("lowDrive")->load() == 18 &&
              std::abs(q->state.getRawParameterValue("lowA5")->load() - .7f) < .001f,
          "inactive band settings and curve recalled");
    check(q->state.getRawParameterValue("bassEnabled")->load() == 1 &&
              q->state.getRawParameterValue("abMatch")->load() == 1,
          "simultaneous stages recalled");
    q->copySnapshot(1, 0);
    check(q->state.getRawParameterValue("lowDrive")->load() == 4, "copy into active slot");
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    auto tree = juce::ValueTree::fromXml(*xml);
    tree.getChildWithName("COMPARE").getChild(0).getChild(0).setProperty("value", 99999, nullptr);
    juce::MemoryBlock invalid, before, after;
    xml = tree.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, invalid);
    q->getStateInformation(before);
    q->setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
    q->getStateInformation(after);
    check(before == after, "invalid inactive state rejected");
    check(q->getBusCount(true) == 1 && q->state.getParameter("scEnabled") == nullptr,
          "sidechain bus and parameters removed");
    auto legacy = p->state.copyState();
    legacy.setProperty("version", 4, nullptr);
    for (int i = legacy.getNumChildren() - 1; i >= 0; --i)
        if (!noclip::parameters::legacyIds().contains(legacy.getChild(i).getProperty("id").toString()))
            legacy.removeChild(i, nullptr);
    xml = legacy.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, state);
    q->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(q->state.getRawParameterValue("lowEnabled")->load() == 0 &&
              q->state.getRawParameterValue("masterEnabled")->load() == 1,
          "legacy initializes bands off");
    // Reconstruct a real schema-5 comparison with different A/B master drives.
    auto oldFive = legacy.createCopy();
    oldFive.setProperty("version", 5, nullptr);
    const auto append = [](juce::ValueTree root, const char *id, float value)
    {
        juce::ValueTree c("PARAM");
        c.setProperty("id", id, nullptr);
        c.setProperty("value", value, nullptr);
        root.appendChild(c, nullptr);
    };
    append(oldFive, "scSensitivity", -24);
    append(oldFive, "scAmount", 6);
    append(oldFive, "scAttack", 5);
    append(oldFive, "scRelease", 150);
    append(oldFive, "scEnabled", 1);
    append(oldFive, "abMatch", 1);
    oldFive.getChildWithProperty("id", "driveDb").setProperty("value", 7, nullptr);
    juce::ValueTree compare("COMPARE");
    compare.setProperty("active", 1, nullptr);
    for (int slot = 0; slot < 2; ++slot)
    {
        juce::ValueTree bank("SLOT");
        bank.setProperty("index", slot, nullptr);
        for (auto child : oldFive)
            if (child.getProperty("id").toString() != "abMatch")
                bank.appendChild(child.createCopy(), nullptr);
        bank.getChildWithProperty("id", "driveDb").setProperty("value", slot == 0 ? 19 : 7, nullptr);
        compare.appendChild(bank, nullptr);
    }
    oldFive.appendChild(compare, nullptr);
    xml = oldFive.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, state);
    q->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(q->selectedSnapshot() == 1 &&
              std::abs(q->state.getRawParameterValue("masterDrive")->load() - 7) < .001f,
          "schema-5 active snapshot migrates");
    q->selectSnapshot(0);
    check(std::abs(q->state.getRawParameterValue("masterDrive")->load() - 19) < .001f,
          "schema-5 inactive snapshot migrates independently");
    q->setParameter("abMatch", 0);
    // The view is not part of DSP state. Exercise every tab and compare complete serialized state.
    q->prepareToPlay(48000, 256);
    q->getStateInformation(before);
    auto editor = std::make_unique<NoclipEditor>(*q);
    editor->setSize(1000, 900);
    for (int page = 0; page < 5; ++page)
    {
        editor->selectPage(page);
        q->getStateInformation(after);
        check(before == after, "tab navigation never changes parameters or A/B snapshots");
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        auto file =
            juce::File::getCurrentWorkingDirectory().getChildFile("workspace-" + juce::String(page) + ".png");
        auto out = file.createOutputStream();
        if (out)
        {
            out->setPosition(0);
            out->truncate();
            juce::PNGImageFormat png;
            png.writeImageToStream(image, *out);
        }
    }
    // Explicit 808 -> every other tab theme regression, with 808 still enabled.
    q->setParameter("bassEnabled", 1);
    q->getStateInformation(before);
    for (int target = 0; target < 4; ++target)
    {
        editor->selectPage(4);
        auto &blue = static_cast<noclip::ui::TacticalLookAndFeel &>(editor->getLookAndFeel());
        check(blue.sand == juce::Colour(0xff788e9d), "808 uses blue palette");
        editor->selectPage(target);
        auto &sand = static_cast<noclip::ui::TacticalLookAndFeel &>(editor->getLookAndFeel());
        check(sand.sand == noclip::ui::sand && sand.ink == noclip::ui::ink && sand.green == noclip::ui::green,
              "leaving 808 restores complete sand/green palette");
        q->getStateInformation(after);
        check(before == after, "theme navigation leaves enabled 808 and DSP untouched");
    }
    auto returned = editor->createComponentSnapshot(editor->getLocalBounds());
    auto out = juce::File::getCurrentWorkingDirectory()
                   .getChildFile("workspace-return-from-808.png")
                   .createOutputStream();
    if (out)
    {
        out->setPosition(0);
        out->truncate();
        juce::PNGImageFormat png;
        png.writeImageToStream(returned, *out);
    }
    // Independent stages: low distortion should alter bass while high processing changes highs.
    noclip::Parameters params;
    params.clipEnabled = true;
    params.modular.active = true;
    params.modular.stages[1].enabled = true;
    params.modular.stages[1].drive = 24;
    noclip::AudioEngine low, identity;
    low.prepare(48000, params);
    params.modular.stages[1].drive = 0;
    params.modular.stages[1].clip = false;
    identity.prepare(48000, params);
    double difference = 0;
    std::array<float, 256> clean;
    float *reference[]{clean.data()};
    for (int block = 0; block < 100; ++block)
    {
        for (int i = 0; i < 256; ++i)
            l[i] = clean[i] = .2f * std::sin(static_cast<float>(block * 256 + i) * .008f);
        low.process(channels, 1, 256);
        identity.process(reference, 1, 256);
        if (block > 20)
            for (int i = 0; i < 256; ++i)
                difference += std::abs(l[i] - clean[i]);
    }
    check(difference > 100, "low stage processes its band");
    params.modular.stages[1].enabled = false;
    params.modular.stages[0].enabled = false;
    params.modular.bass = false;
    identity.prepare(48000, params);
    l.fill(.2f);
    identity.process(channels, 1, 256);
    check(std::abs(l.back() - .2f) < 1e-6f, "all sections off is identity below ceiling");
    // Isolated 808 retains the original six-control processing at default clip settings.
    noclip::Parameters oldBass;
    oldBass.clipEnabled = true;
    oldBass.advanced.routing = 2;
    oldBass.advanced.bassDrive = 18;
    oldBass.advanced.punch = .5f;
    oldBass.advanced.sub = .8f;
    noclip::AudioEngine old808, new808;
    old808.prepare(48000, oldBass);
    auto newBass = oldBass;
    newBass.modular.active = true;
    newBass.modular.bass = true;
    new808.prepare(48000, newBass);
    double bassError = 0;
    for (int block = 0; block < 100; ++block)
    {
        for (int i = 0; i < 256; ++i)
            l[i] = clean[i] = .2f * std::sin(static_cast<float>(block * 256 + i) * .013f);
        old808.process(channels, 1, 256);
        new808.process(reference, 1, 256);
        for (int i = 0; i < 256; ++i)
            bassError = std::max(bassError, static_cast<double>(std::abs(l[i] - clean[i])));
    }
    check(bassError < 1e-5, "808 core retains original sound when isolated");
    // Two independently edited LUTs must produce different band results.
    params = {};
    params.clipEnabled = true;
    params.modular.active = true;
    params.modular.stages[1].enabled = true;
    params.modular.stages[1].clip = false;
    params.modular.stages[1].custom = true;
    low.prepare(48000, params);
    identity.prepare(48000, params);
    auto points = noclip::identityPoints;
    for (auto &x : points)
        x *= .2f;
    low.setStageTable(1, noclip::CurveTable::make(points, points), true);
    identity.setStageTable(1, curve, true);
    difference = 0;
    for (int block = 0; block < 100; ++block)
    {
        for (int i = 0; i < 256; ++i)
            l[i] = clean[i] = .2f * std::sin(static_cast<float>(block * 256 + i) * .008f);
        low.process(channels, 1, 256);
        identity.process(reference, 1, 256);
        if (block > 20)
            for (int i = 0; i < 256; ++i)
                difference += std::abs(l[i] - clean[i]);
    }
    check(difference > 100, "band custom LUT independently changes audio");
    // Splitting a block must not change linked filter/control state.
    params.modular.stages[0].enabled = true;
    params.modular.stages[0].drive = 3;
    params.modular.stages[2].enabled = true;
    params.modular.stages[2].custom = true;
    params.modular.bass = true;
    noclip::AudioEngine whole, partitioned;
    whole.prepare(48000, params);
    partitioned.prepare(48000, params);
    std::array<float, 4096> one, two;
    for (size_t i = 0; i < one.size(); ++i)
        one[i] = two[i] = .1f * std::sin(static_cast<float>(i) * .041f);
    float *first[]{one.data()};
    whole.process(first, 1, 4096);
    for (int offset = 0; offset < 4096;)
    {
        int n = std::min(37, 4096 - offset);
        float *part[]{two.data() + offset};
        partitioned.process(part, 1, n);
        offset += n;
    }
    check(one == two, "modular DSP invariant to host block partitioning");
    std::cout << "Modular/A-B failures: " << failures << '\n';
    return failures;
}
