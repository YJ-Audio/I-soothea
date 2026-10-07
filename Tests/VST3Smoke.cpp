#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <crtdbg.h>
#define NOMINMAX
#include <windows.h>

static thread_local bool watching = false;
static int allocations = 0;
static int allocationHook(int type, void*, std::size_t, int, long, const unsigned char*, int)
{
    if (watching && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations;
    return 1;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    _CrtSetAllocHook(allocationHook);
    if (argc != 2) return 1;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile(types, juce::String::fromUTF8(argv[1]));
    if (types.size() != 1 || types[0]->isInstrument || types[0]->name != "I Soothea" || types[0]->manufacturerName != "YJ Audio") return 2;
    juce::String error;
    auto plugin = format.createInstanceFromDescription(*types[0], 48000, 256, error);
    if (!plugin) { std::cerr << error << '\n'; return 3; }
    auto find = [&](const juce::String& name) -> juce::AudioProcessorParameter*
    {
        for (auto* parameter : plugin->getParameters()) if (parameter->getName(64) == name) return parameter;
        return nullptr;
    };
    auto* depth = find("Depth");
    auto* detail = find("Detail");
    auto* delta = find("Delta");
    if (!depth || !detail || !delta) return 4;
    depth->setValueNotifyingHost(0);
    juce::MidiBuffer midi;
    double maximumError = 0;
    auto* linearPhase = find("Linear phase"); if (!linearPhase || linearPhase->isAutomatable()) return 29;
    auto* lowLatency = find("Low latency"); if (!lowLatency || lowLatency->isAutomatable()) return 24;
    for (bool linear : {false, true}) for (bool low : {false, true}) for (int channels : {1, 2})
    {
        linearPhase->setValueNotifyingHost(linear ? 1.0f : 0.0f);
        lowLatency->setValueNotifyingHost(low ? 1.0f : 0.0f);
        const int latency = linear ? 3072 : low ? 2048 : 4096;
        auto layout = plugin->getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
        layout.outputBuses.set(0, juce::AudioChannelSet::canonicalChannelSet(channels));
        if (!plugin->setBusesLayout(layout)) return 5;
        for (int block : {1, 17, 127, 1024, 2049})
        {
            plugin->setRateAndBufferSizeDetails(48000, block);
            plugin->prepareToPlay(48000, block);
            if (plugin->getLatencySamples() != latency) return 6;
            juce::AudioBuffer<float> audio(channels, block);
            int time = 0;
            while (time < 16000)
            {
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < block; ++i)
                        audio.setSample(c, i, static_cast<float>(0.2 * std::sin(0.13 * (time + i) * (c + 1))));
                watching = true;
                plugin->processBlock(audio, midi);
                watching = false;
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < block; ++i)
                    {
                        const auto expected = time + i < latency ? 0 : 0.2 * std::sin(0.13 * (time + i - latency) * (c + 1));
                        const float actual = audio.getSample(c, i);
                        if (!std::isfinite(actual)) return 7;
                        maximumError = std::max(maximumError, std::abs(actual - expected));
                    }
                time += block;
            }
            plugin->releaseResources();
        }
    }
    linearPhase->setValueNotifyingHost(0);
    lowLatency->setValueNotifyingHost(0);
    // The actual VST3 must give the same active output across changing host blocks,
    // including zero-sized calls. This detects per-block reset/discontinuity bugs.
    depth->setValueNotifyingHost(0.5f); detail->setValueNotifyingHost(0.5f);
    const int length = 48000;
    auto render = [&](bool varyingBlocks, bool deltaMode, float amplitude = 0.3f)
    {
        delta->setValueNotifyingHost(deltaMode ? 1.0f : 0.0f);
        plugin->prepareToPlay(48000, 2049);
        std::vector<float> result(length);
        const int blockSizes[] = {0, 1, 17, 127, 511, 2049, 64};
        juce::AudioBuffer<float> audio(2, 2049);
        int position = 0, iteration = 0;
        while (position < length)
        {
            const int block = std::min(length - position, varyingBlocks ? blockSizes[iteration++ % 7] : 256);
            audio.setSize(2, block, false, false, true);
            for (int i = 0; i < block; ++i)
                for (int c = 0; c < 2; ++c)
                    audio.setSample(c, i, static_cast<float>(amplitude * std::sin(6.283185307179586 * (position + i) / 48)));
            watching = true;
            plugin->processBlock(audio, midi);
            watching = false;
            for (int i = 0; i < block; ++i) result[position + i] = audio.getSample(0, i);
            position += block;
        }
        plugin->releaseResources();
        return result;
    };
    const auto fixed = render(false, false);
    const auto variable = render(true, false);
    const auto removed = render(true, true);
    double blockNull = 0, deltaNull = 0;
    for (int i = 0; i < length; ++i)
    {
        blockNull = std::max(blockNull, static_cast<double>(std::abs(fixed[i] - variable[i])));
        const double aligned = i < 4096 ? 0 : 0.3 * std::sin(6.283185307179586 * (i - 4096) / 48);
        deltaNull = std::max(deltaNull, std::abs(aligned - variable[i] - removed[i]));
    }
    std::cout << "VST3 active block null=" << blockNull << " Delta null=" << deltaNull << '\n';
    if (blockNull > 1.0e-6 || deltaNull > 1.0e-6) return 11;
    auto* mode = find("Mode"); if (!mode) return 16;
    mode->setValueNotifyingHost(1);
    const auto hardHigh = render(false, false);
    const auto hardLow = render(true, false, 0.03f);
    double highEnergy = 0, lowEnergy = 0;
    for (int i = 24000; i < length; ++i) { highEnergy += hardHigh[i] * hardHigh[i]; lowEnergy += hardLow[i] * hardLow[i]; }
    if (highEnergy / 100 >= lowEnergy * 0.5) return 17;
    std::cout << "VST3 Hard stronger input receives more relative reduction\n";
    auto* stereoMode = find("Stereo mode"); auto* link = find("Stereo link"); auto* focus = find("Band 3 Focus");
    if (!stereoMode || !link || !focus) return 19;
    stereoMode->setValueNotifyingHost(1); link->setValueNotifyingHost(1); focus->setValueNotifyingHost(0.2f);
    auto* quality = find("Quality"); if (!quality) return 22;
    quality->setValueNotifyingHost(1);
    const auto msFixed = render(false, false), msVariable = render(true, false), msDelta = render(true, true);
    for (int i = 0; i < length; ++i)
    {
        const double aligned = i < 4096 ? 0 : 0.3 * std::sin(6.283185307179586 * (i - 4096) / 48);
        if (std::abs(msFixed[i] - msVariable[i]) > 1e-6 || std::abs(aligned - msVariable[i] - msDelta[i]) > 1e-6) return 20;
    }
    lowLatency->setValueNotifyingHost(1);
    const auto lowFixed = render(false, false), lowVariable = render(true, false), lowDelta = render(true, true);
    for (int i = 0; i < length; ++i)
    {
        const double aligned = i < 2048 ? 0 : 0.3 * std::sin(6.283185307179586 * (i - 2048) / 48);
        if (std::abs(lowFixed[i] - lowVariable[i]) > 1e-6 || std::abs(aligned - lowVariable[i] - lowDelta[i]) > 1e-6) return 25;
    }
    linearPhase->setValueNotifyingHost(1);
    const auto linearFixed = render(false, false), linearVariable = render(true, false), linearDelta = render(true, true);
    for (int i = 0; i < length; ++i)
    {
        const double aligned = i < 3072 ? 0 : 0.3 * std::sin(6.283185307179586 * (i - 3072) / 48);
        if (std::abs(linearFixed[i] - linearVariable[i]) > 1e-6 || std::abs(aligned - linearVariable[i] - linearDelta[i]) > 1e-6) return 30;
    }
    // The advertised tail must include the retained analysis history as well as synthesis latency.
    delta->setValueNotifyingHost(0);
    for (bool linear : {false, true}) for (bool low : {false, true})
    {
        linearPhase->setValueNotifyingHost(linear ? 1.0f : 0.0f);
        lowLatency->setValueNotifyingHost(low ? 1.0f : 0.0f); plugin->prepareToPlay(48000, 64);
        const int tail = static_cast<int>(std::ceil(plugin->getTailLengthSeconds() * 48000));
        if (tail < (low ? 2048 : 4096)) return 27;
        juce::AudioBuffer<float> drain(2, 64);
        for (int position = 0; position < tail + 4096; position += 64)
        {
            drain.clear(); if (position == 0) { drain.setSample(0, 0, 1); drain.setSample(1, 0, 0.5f); }
            watching = true; plugin->processBlock(drain, midi); watching = false;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 64; ++i)
                if (position + i >= tail && std::abs(drain.getSample(c, i)) > 1e-8f) { std::cerr << "tail failure linear=" << linear << " reported=" << tail << " position=" << position+i << " residual=" << drain.getSample(c,i) << std::endl; return 28; }
        }
        plugin->releaseResources();
    }
    depth->setValueNotifyingHost(0.7f); detail->setValueNotifyingHost(0.9f); delta->setValueNotifyingHost(1);
    auto* bandWeight = find("Band 3 Weight (dB)");
    if (!bandWeight) return 14;
    bandWeight->setValueNotifyingHost(0.625f);
    juce::MemoryBlock state; plugin->getStateInformation(state);
    mode->setValueNotifyingHost(0);
    bandWeight->setValueNotifyingHost(0.5f);
    depth->setValueNotifyingHost(0); delta->setValueNotifyingHost(0);
    linearPhase->setValueNotifyingHost(0);
    lowLatency->setValueNotifyingHost(0);
    quality->setValueNotifyingHost(0);
    plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    if (lowLatency->getValue() < 0.5f) return 26;
    if (linearPhase->getValue() < 0.5f) return 31;
    if (quality->getValue() < 0.5f) return 23;
    if (stereoMode->getValue() < 0.5f || link->getValue() < 0.99f || std::abs(focus->getValue() - 0.2f) > 0.001f) return 21;
    if (mode->getValue() < 0.5f) return 18;
    if (std::abs(depth->getValue() - 0.7f) > 0.001 || delta->getValue() < 0.5f) return 8;
    if (std::abs(bandWeight->getValue() - 0.625f) > 0.001f) return 15;
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());
    if (!editor || editor->getWidth() != 1100 || editor->getHeight() != 680) return 9;
    auto pump = []
    {
        const auto until = juce::Time::getMillisecondCounter() + 70;
        while (juce::Time::getMillisecondCounter() < until)
        {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
            juce::Thread::sleep(1);
        }
    };
    depth->setValueNotifyingHost(0.5f); detail->setValueNotifyingHost(0.5f); delta->setValueNotifyingHost(0);
    pump();
    plugin->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> preview(2, 256);
    for (int block = 0; block < 200; ++block)
    {
        if (block % 11 == 0) quality->setValueNotifyingHost((block / 11) % 2 ? 1.0f : 0.0f);
        if (block % 17 == 0) { stereoMode->setValueNotifyingHost((block / 17) % 2 ? 1.0f : 0.0f); focus->setValueNotifyingHost((block / 17) % 2 ? 0.0f : 1.0f); }
        if (block == 80) mode->setValueNotifyingHost(0);
        if (block == 140) mode->setValueNotifyingHost(1);
        for (int i = 0; i < 256; ++i)
        {
            double value = 0;
            for (double frequency : {500.0, 1000.0, 2000.0}) value += 0.15 * std::sin(6.283185307179586 * frequency * (block * 256 + i) / 48000);
            for (int c = 0; c < 2; ++c) preview.setSample(c, i, static_cast<float>(value));
        }
        watching = true; plugin->processBlock(preview, midi); watching = false;
        if (block % 50 == 49) pump();
    }
    editor->setSize(880, 544);
    plugin->releaseResources();
    editor.reset();
    plugin.reset();
    std::cout << "VST3 discovery/load, mono/stereo, latency, block sizes, state, editor: max neutral error=" << maximumError << " callback allocations=" << allocations << '\n';
    return maximumError < 2.0e-6 && allocations == 0 ? 0 : 10;
}
