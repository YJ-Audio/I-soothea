#pragma once
#include "DetailProcessor.h"
#include "BandWeighting.h"
#include "HardDetector.h"

namespace soothe
{
struct Settings
{
    float depth = 5, detail = 5, attack = Tuning::attackMs, release = Tuning::releaseMs;
    float mix = 1, outputDb = 0;
    bool delta = false, bypass = false;
    bool hard = false;
    bool midSide = false;
    bool highQuality = false;
    float link = 0;
    Bands bands = defaultBands();
};

template <std::size_t N>
class BasicEngine
{
public:
    using STFT = BasicSTFT<4096, N>;
    void prepare(double sampleRate, const Settings& settings = {}) noexcept
    {
        stft.reset(settings.highQuality ? STFT::hop / 2 : STFT::hop); detector.prepare(sampleRate); detailProcessor.prepare();
        weighting.prepare(sampleRate);
        hardDetector.prepare(sampleRate);
        dry.fill(0); dryPosition = 0;
        parameterCoefficient = std::exp(-1.0f / static_cast<float>(0.005 * sampleRate));
        setSettings(settings);
        mix = target.mix; delta = target.delta ? 1.0f : 0.0f;
        bypass = target.bypass ? 1.0f : 0.0f; gain = std::pow(10.0f, target.outputDb / 20);
    }
    void setSettings(const Settings& settings) noexcept
    {
        target = settings;
        stft.setHighQuality(settings.highQuality);
        targetGain = std::pow(10.0f, std::clamp(settings.outputDb, -24.0f, 12.0f) / 20);
    }
    float tick(float input) noexcept
    {
        if (!std::isfinite(input)) input = 0;
        const float aligned = dry[dryPosition];
        dry[dryPosition] = input;
        dryPosition = (dryPosition + 1) % dry.size();
        const float wet = stft.tick(input, [this](auto& spectrum) noexcept
        {
            detector.setTiming(target.attack, target.release, stft.frameInterval());
            if (target.hard)
            {
                detector.analyzeMagnitude(spectrum);
                hardDetector.analyze(detector.magnitude);
            }
            else detector.analyze(spectrum);
            detailProcessor.process(target.hard ? hardDetector.raw : detector.raw, target.detail);
            weighting.apply(detailProcessor.result, target.bands);
            detector.attenuate(spectrum, weighting.result, target.depth);
        });
        smooth(mix, std::clamp(target.mix, 0.0f, 1.0f));
        smooth(delta, target.delta ? 1.0f : 0.0f);
        smooth(bypass, target.bypass ? 1.0f : 0.0f);
        smooth(gain, targetGain);
        const float normal = aligned + mix * (wet - aligned);
        const float monitored = normal + delta * ((aligned - wet) - normal);
        return (1 - bypass) * monitored * gain + bypass * aligned;
    }
    const SoftDetector& diagnostics() const noexcept { return detector; }
private:
    void smooth(float& current, float wanted) const noexcept
    {
        current = wanted + parameterCoefficient * (current - wanted);
        if (std::abs(current - wanted) < 1.0e-6f) current = wanted;
    }
    STFT stft;
    SoftDetector detector;
    DetailProcessor detailProcessor;
    BandWeighting weighting;
    HardDetector hardDetector;
    std::array<float, STFT::latency> dry{};
    std::size_t dryPosition = 0;
    Settings target;
    float parameterCoefficient = 0, mix = 1, delta = 0, bypass = 0, gain = 1, targetGain = 1;
};
using Engine = BasicEngine<4096>;
}
