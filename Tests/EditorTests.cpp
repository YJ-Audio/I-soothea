#include "PluginEditor.h"
#define NOMINMAX
#include <windows.h>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto processor = std::make_unique<SootheCloneProcessor>();
    auto editor = std::make_unique<SootheCloneEditor>(*processor);
    auto pump = []
    {
        const auto end = juce::Time::getMillisecondCounter() + 60;
        while (juce::Time::getMillisecondCounter() < end)
        {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
            juce::Thread::sleep(1);
        }
    };
    juce::Slider* depth = nullptr;
    juce::TextButton* reset = nullptr;
    for (auto* child : editor->getChildren())
    {
        if (child->getName() == "Depth") depth = dynamic_cast<juce::Slider*>(child);
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) reset = button;
    }
    if (!depth || !reset) return 1;
    depth->setValue(7.5, juce::sendNotificationSync);
    if (processor->parameters.getRawParameterValue("depth")->load() != 7.5f) return 2;
    reset->triggerClick(); pump();
    if (processor->parameters.getRawParameterValue("depth")->load() != 5) return 3;
    juce::ToggleButton* hardButton = nullptr;
    juce::ToggleButton* softButton = nullptr;
    for (auto* child : editor->getChildren())
    {
        if (child->getName() == "Hard mode") hardButton = dynamic_cast<juce::ToggleButton*>(child);
        if (child->getName() == "Soft mode") softButton = dynamic_cast<juce::ToggleButton*>(child);
    }
    if (!hardButton || !softButton) return 11;
    hardButton->triggerClick(); pump();
    if (!processor->isHardMode() || !hardButton->getToggleState() || softButton->getToggleState()) return 12;
    juce::MemoryBlock modeState; processor->getStateInformation(modeState);
    softButton->triggerClick(); pump();
    if (processor->isHardMode()) return 13;
    processor->setStateInformation(modeState.getData(), static_cast<int>(modeState.getSize())); pump();
    if (!processor->isHardMode() || !hardButton->getToggleState()) return 14;
    auto preHard = processor->parameters.copyState();
    preHard.removeChild(preHard.getChildWithProperty("id", "mode"), nullptr);
    juce::MemoryBlock preHardState; juce::AudioProcessor::copyXmlToBinary(*preHard.createXml(), preHardState);
    processor->setStateInformation(preHardState.getData(), static_cast<int>(preHardState.getSize())); pump();
    if (processor->isHardMode() || !softButton->getToggleState()) return 15;
    juce::ComboBox* stereo = nullptr;
    juce::Slider* link = nullptr;
    juce::Slider* focus = nullptr;
    for (auto* child : editor->getChildren())
    {
        if (child->getName() == "Stereo mode") stereo = dynamic_cast<juce::ComboBox*>(child);
        if (child->getName() == "Stereo link") link = dynamic_cast<juce::Slider*>(child);
        if (child->getName() == "Band focus") focus = dynamic_cast<juce::Slider*>(child);
    }
    if (!stereo || !link || !focus) return 20;
    stereo->setSelectedId(2, juce::sendNotificationSync); link->setValue(0.75, juce::sendNotificationSync); focus->setValue(-0.6, juce::sendNotificationSync);
    if (!processor->isMidSide() || std::abs(processor->getBands()[2].focus + 0.6f) > 0.001f
        || std::abs(processor->parameters.getRawParameterValue("link")->load() - 0.75f) > 0.001f) return 21;
    juce::MemoryBlock stereoState; processor->getStateInformation(stereoState);
    reset->triggerClick(); pump(); processor->setStateInformation(stereoState.getData(), static_cast<int>(stereoState.getSize())); pump();
    if (!processor->isMidSide() || std::abs(focus->getValue() + 0.6) > 0.001 || std::abs(link->getValue() - 0.75) > 0.001) return 22;
    auto preStereo = processor->parameters.copyState();
    for (int i = preStereo.getNumChildren(); --i >= 0;)
    {
        const auto id = preStereo.getChild(i).getProperty("id").toString();
        if (id == "stereoMode" || id == "link" || id.endsWith("_focus")) preStereo.removeChild(i, nullptr);
    }
    juce::MemoryBlock legacyStereo; juce::AudioProcessor::copyXmlToBinary(*preStereo.createXml(), legacyStereo);
    processor->setStateInformation(legacyStereo.getData(), static_cast<int>(legacyStereo.getSize())); pump();
    if (processor->isMidSide() || link->getValue() != 0 || focus->getValue() != 0) return 23;
    auto layout = processor->getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::mono()); layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    if (!processor->setBusesLayout(layout)) return 24;
    pump(); if (stereo->isEnabled() || link->isEnabled() || focus->isEnabled()) return 25;
    layout.inputBuses.set(0, juce::AudioChannelSet::stereo()); layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    if (!processor->setBusesLayout(layout)) return 26;
    pump(); if (!stereo->isEnabled() || !link->isEnabled() || !focus->isEnabled()) return 27;
    juce::ComboBox* quality = nullptr;
    for (auto* child : editor->getChildren())
        if (child->getName() == "Quality") quality = dynamic_cast<juce::ComboBox*>(child);
    if (!quality || quality->getSelectedId() != 1) return 28;
    quality->setSelectedId(2, juce::sendNotificationSync);
    if (processor->parameters.getRawParameterValue("quality")->load() != 1) return 29;
    juce::MemoryBlock qualityState; processor->getStateInformation(qualityState);
    reset->triggerClick(); pump();
    if (quality->getSelectedId() != 1) return 30;
    processor->setStateInformation(qualityState.getData(), static_cast<int>(qualityState.getSize())); pump();
    if (quality->getSelectedId() != 2) return 31;
    auto preQuality = processor->parameters.copyState(); preQuality.removeChild(preQuality.getChildWithProperty("id", "quality"), nullptr);
    juce::MemoryBlock legacyQuality; juce::AudioProcessor::copyXmlToBinary(*preQuality.createXml(), legacyQuality);
    processor->setStateInformation(legacyQuality.getData(), static_cast<int>(legacyQuality.getSize())); pump();
    if (quality->getSelectedId() != 1) return 32;
    juce::ToggleButton* lowLatency = nullptr;
    for (auto* child : editor->getChildren())
        if (child->getName() == "Low latency") lowLatency = dynamic_cast<juce::ToggleButton*>(child);
    if (!lowLatency || processor->parameters.getParameter("lowLatency")->isAutomatable()) return 33;
    lowLatency->triggerClick(); pump();
    if (!processor->isLatencyChangePending() || processor->getLatencySamples() != 4096 || lowLatency->getButtonText() != "reload to apply") return 34;
    juce::MemoryBlock lowState; processor->getStateInformation(lowState);
    processor->prepareToPlay(48000, 256); pump();
    if (!processor->isLowLatencyActive() || processor->isLatencyChangePending() || processor->getLatencySamples() != 2048) return 35;
    lowLatency->triggerClick(); pump(); processor->reset();
    if (!processor->isLatencyChangePending() || processor->getLatencySamples() != 2048) return 36;
    processor->setStateInformation(lowState.getData(), static_cast<int>(lowState.getSize())); pump();
    if (processor->isLatencyChangePending() || !lowLatency->getToggleState()) return 37;
    auto preLow = processor->parameters.copyState(); preLow.removeChild(preLow.getChildWithProperty("id", "lowLatency"), nullptr);
    juce::MemoryBlock legacyLow; juce::AudioProcessor::copyXmlToBinary(*preLow.createXml(), legacyLow);
    processor->setStateInformation(legacyLow.getData(), static_cast<int>(legacyLow.getSize())); pump();
    if (!processor->isLatencyChangePending() || lowLatency->getToggleState()) return 38;
    processor->prepareToPlay(48000, 256); pump();
    if (processor->isLowLatencyActive() || processor->isLatencyChangePending() || processor->getLatencySamples() != 4096) return 39;
    juce::ToggleButton* linearPhase = nullptr;
    for (auto* child : editor->getChildren())
        if (child->getName() == "Linear phase") linearPhase = dynamic_cast<juce::ToggleButton*>(child);
    if (!linearPhase || processor->parameters.getParameter("linearPhase")->isAutomatable()) return 40;
    linearPhase->triggerClick(); pump();
    if (!processor->isLatencyChangePending() || lowLatency->isEnabled() || linearPhase->getButtonText() != "reload to apply") return 41;
    juce::MemoryBlock linearState; processor->getStateInformation(linearState);
    processor->prepareToPlay(48000, 256); pump();
    if (!processor->isLinearPhaseActive() || processor->isLatencyChangePending() || processor->getLatencySamples() != 3072) return 42;
    processor->parameters.getParameter("lowLatency")->setValueNotifyingHost(1); pump();
    if (processor->isLatencyChangePending() || processor->isLowLatencyActive()) return 43;
    linearPhase->triggerClick(); pump(); processor->reset();
    if (!processor->isLatencyChangePending() || processor->getLatencySamples() != 3072) return 44;
    processor->prepareToPlay(48000, 256); pump();
    if (!processor->isLowLatencyActive() || processor->isLinearPhaseActive() || processor->getLatencySamples() != 2048) return 45;
    processor->setStateInformation(linearState.getData(), static_cast<int>(linearState.getSize())); pump();
    if (!processor->isLatencyChangePending() || !linearPhase->getToggleState()) return 46;
    processor->prepareToPlay(48000, 256);
    auto preLinear = processor->parameters.copyState(); preLinear.removeChild(preLinear.getChildWithProperty("id", "linearPhase"), nullptr);
    juce::MemoryBlock legacyLinear; juce::AudioProcessor::copyXmlToBinary(*preLinear.createXml(), legacyLinear);
    processor->setStateInformation(legacyLinear.getData(), static_cast<int>(legacyLinear.getSize())); pump();
    if (!processor->isLatencyChangePending() || linearPhase->getToggleState()) return 47;
    processor->prepareToPlay(48000, 256); pump();
    if (processor->isLinearPhaseActive() || processor->isLowLatencyActive() || processor->getLatencySamples() != 4096) return 48;
    auto mouse = [&](juce::Point<float> point, bool dragged)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            editor.get(), editor.get(), juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, dragged);
    };
    const auto x = [](float frequency) { return 246 + 832 * std::log(frequency / 20) / std::log(1000.0f); };
    const float zero = 87 + 454 * 0.5f;
    editor->mouseDown(mouse({x(1000), zero}, false));
    editor->mouseDrag(mouse({x(2000), zero - 454.0f / 8}, true));
    editor->mouseUp(mouse({x(2000), zero - 454.0f / 8}, true));
    auto bands = processor->getBands();
    if (std::abs(bands[2].frequency - 2000) > 0.01 || std::abs(bands[2].amountDb - 6) > 0.01) return 7;
    juce::MouseWheelDetails wheel{}; wheel.deltaY = 0.5f;
    editor->mouseWheelMove(mouse({x(2000), zero - 454.0f / 8}, false), wheel);
    if (processor->getBands()[2].widthOctaves <= 1) return 8;
    juce::MemoryBlock state; processor->getStateInformation(state);
    reset->triggerClick(); pump(); processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    if (std::abs(processor->getBands()[2].frequency - 2000) > 0.01) return 9;
    auto legacy = processor->parameters.copyState();
    for (int i = legacy.getNumChildren(); --i >= 0;)
        if (legacy.getChild(i).getProperty("id").toString().startsWith("band")) legacy.removeChild(i, nullptr);
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), oldState);
    processor->setStateInformation(oldState.getData(), static_cast<int>(oldState.getSize()));
    const auto restoredBands = processor->getBands(); const auto defaults = soothe::defaultBands();
    for (int i = 0; i < 5; ++i)
        if (restoredBands[i].enabled != defaults[i].enabled || restoredBands[i].shape != defaults[i].shape
            || std::abs(restoredBands[i].frequency - defaults[i].frequency) > 0.02f
            || std::abs(restoredBands[i].amountDb) > 0.001f
            || std::abs(restoredBands[i].widthOctaves - defaults[i].widthOctaves) > 0.001f
            || std::abs(restoredBands[i].slope - defaults[i].slope) > 0.001f) return 10;
    auto set = [&](int band, const char* field, float value)
    {
        auto* p = processor->parameters.getParameter(processor->bandID(band, field)); p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    auto doubleClick = [&](juce::Point<float> point)
    {
        editor->mouseDown(mouse(point, false)); editor->mouseUp(mouse(point, false));
        editor->mouseDown(mouse(point, false)); editor->mouseDoubleClick(mouse(point, false));
        editor->mouseUp(mouse(point, false));
    };
    const auto beforePoints = processor->getBands();
    doubleClick({120, 200});
    if (processor->getBands() != beforePoints) return 49;
    doubleClick({x(600), zero - 454.0f/8});
    auto added = processor->getBands();
    if (!added[0].enabled || added[0].shape != soothe::BandShape::bell
        || std::abs(added[0].frequency - 600) > 0.02f || std::abs(added[0].amountDb - 6) > 0.01f) return 50;
    doubleClick({x(600), zero - 454.0f/8});
    if (processor->getBands()[0].enabled) return 51;
    // Hidden points cannot be dragged back into existence.
    editor->mouseDown(mouse({x(600), zero - 454.0f/8}, false));
    editor->mouseDrag(mouse({x(800), zero}, true)); editor->mouseUp(mouse({x(800), zero}, true));
    if (processor->getBands()[0].enabled) return 52;
    editor->setSize(880, 544);
    doubleClick(juce::Point<float>{x(800), zero + 454.0f/8} * 0.8f);
    editor->setSize(1100, 680);
    added = processor->getBands();
    if (!added[0].enabled || std::abs(added[0].frequency - 800) > 0.02f || std::abs(added[0].amountDb + 6) > 0.01f) return 53;
    doubleClick({x(8000), zero - 454.0f/8});
    const auto fullPoints = processor->getBands();
    doubleClick({x(12000), zero - 100});
    if (processor->getBands() != fullPoints || !fullPoints[4].enabled) return 54;
    set(4, "shape", 4);
    doubleClick({x(8000), zero});
    if (processor->getBands()[4].enabled) return 55;
    const auto savedPoints = processor->getBands();
    juce::MemoryBlock pointsState; processor->getStateInformation(pointsState);
    reset->triggerClick(); pump();
    processor->setStateInformation(pointsState.getData(), static_cast<int>(pointsState.getSize())); pump();
    if (processor->getBands() != savedPoints) return 56;
    reset->triggerClick(); pump();
    set(0, "enabled", 1); set(4, "enabled", 1); set(3, "amount", 6); set(3, "width", 2);
    editor->mouseDown(mouse({x(4000), zero - 454.0f / 8}, false));
    editor->mouseUp(mouse({x(4000), zero - 454.0f / 8}, false));
    hardButton->triggerClick();
    stereo->setSelectedId(2, juce::sendNotificationSync);
    link->setValue(0.75, juce::sendNotificationSync);
    focus->setValue(-0.6, juce::sendNotificationSync);
    pump();
    quality->setSelectedId(2, juce::sendNotificationSync);
    lowLatency->triggerClick(); pump();
    linearPhase->triggerClick(); pump();
    processor->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
    for (int block = 0; block < 220; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            double value = 0;
            for (double f : {500.0, 1000.0, 2000.0}) value += 0.15 * std::sin(6.283185307179586 * f * (block * 256 + i) / 48000);
            for (int c = 0; c < 2; ++c) audio.setSample(c, i, static_cast<float>(value));
        }
        processor->processBlock(audio, midi);
        if (block % 50 == 49) pump();
    }
    pump();
    for (int width : {1100, 880})
    {
        editor->setSize(width, width * 680 / 1100);
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        if (image.getPixelAt(300, 100).getAlpha() == 0) return 4;
        auto stream = juce::File::getCurrentWorkingDirectory().getChildFile(width == 1100 ? "SootheClone-UI.png" : "SootheClone-UI-small.png").createOutputStream();
        if (!stream) return 5;
        stream->setPosition(0); stream->truncate();
        if (!juce::PNGImageFormat().writeImageToStream(image, *stream)) return 6;
    }
    std::cout << "Editor Stereo/Link/Focus controls/state/legacy, Soft/Hard controls/state/legacy, parameter attachment, Reset, band drag/wheel, state and legacy migration, live rendering, 1100/880px snapshots passed\n";
}

