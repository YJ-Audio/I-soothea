#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "StereoEngine.h"
#include "LinearPhaseEngine.h"

class SootheCloneProcessor final : public juce::AudioProcessor
{
public:
    SootheCloneProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    struct DisplayFrame
    {
        static constexpr int points = 256;
        std::array<float, points> inputDb{}, reductionDb{};
        float upperFrequency = 20000;
    };
    bool readDisplay(DisplayFrame&) noexcept;
    soothe::Bands getBands() const noexcept;
    bool isLinearPhaseRequested() const noexcept { return linearPhaseValue->load() >= 0.5f; }
    bool isLinearPhaseActive() const noexcept { return activeLinearPhase.load(); }
    bool isLowLatencyActive() const noexcept { return activeLowLatency.load(); }
    bool isLatencyChangePending() const noexcept { return isLinearPhaseRequested() != isLinearPhaseActive() || (!isLinearPhaseRequested() && (lowLatencyValue->load() >= 0.5f) != isLowLatencyActive()); }
    double activeLatencyMs() const noexcept { return (isLinearPhaseActive() ? 3072.0 : isLowLatencyActive() ? 2048.0 : 4096.0) * 1000 / displayRate.load(); }
    bool isMidSide() const noexcept { return stereoValue->load() >= 0.5f; }
    bool isHardMode() const noexcept { return modeValue->load() >= 0.5f; }
    static juce::String bandID(int index, const char* field) { return "band" + juce::String(index) + "_" + field; }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return isLinearPhaseActive() ? 9216.0 / displayRate.load() : activeLatencyMs() / 1000 + 4096.0 / displayRate.load(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    soothe::Settings settings() const noexcept;
    void process(juce::AudioBuffer<float>&, bool);
    soothe::LinearPhaseEngine linearEngine;
    soothe::Engine monoEngine;
    soothe::StereoEngine stereoEngine;
    soothe::BasicEngine<2048> lowMonoEngine;
    soothe::BasicStereoEngine<2048> lowStereoEngine;
    std::array<std::atomic<float>*, 8> values{};
    std::atomic<float>* modeValue = nullptr;
    std::atomic<float>* stereoValue = nullptr;
    std::atomic<float>* linkValue = nullptr;
    std::atomic<float>* qualityValue = nullptr;
    std::atomic<float>* lowLatencyValue = nullptr;
    std::atomic<float>* linearPhaseValue = nullptr;
    std::atomic<bool> activeLinearPhase{false};
    std::atomic<bool> activeLowLatency{false};
    std::atomic<double> displayRate{48000};
    std::array<std::array<std::atomic<float>*, 7>, 5> bandValues{};
    double rate = 48000;
    DisplayFrame display;
    std::array<int, DisplayFrame::points + 1> displayBins{};
    std::atomic<int> displayState{0}; // 0 free, 1 audio writer, 2 ready, 3 GUI reader
    int displayCountdown = 0;
    void publishDisplay(int channels) noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SootheCloneProcessor)
};
