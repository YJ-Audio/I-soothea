#pragma once
#include "STFT.h"
#include <algorithm>

namespace soothe
{
// Experimental constants describe our model, not proprietary implementation facts.
struct Tuning
{
    static constexpr float baselineOctaves = 1.0f;
    static constexpr float relativeFloorDb = 90.0f;
    static constexpr float prominenceKneeDb = 3.0f;
    static constexpr float softRatio = 0.12f;
    static constexpr float maximumReductionDb = 36.0f;
    static constexpr float nominalDepth = 5.0f;
    static constexpr float attackMs = 20.0f, releaseMs = 120.0f;
};

class SoftDetector
{
public:
    using Spectrum = FFT::Spectrum;
    static constexpr std::size_t bins = FFT::size / 2 + 1;
    using Curve = std::array<float, bins>;
    void prepare(double sampleRate) noexcept
    {
        for (std::size_t k = 0; k < bins; ++k)
        {
            const auto radius = std::max(4, static_cast<int>(k * (std::pow(2.0f, Tuning::baselineOctaves / 2) - 1)));
            low[k] = static_cast<std::size_t>(std::max(0, static_cast<int>(k) - radius));
            high[k] = std::min(bins - 1, k + radius);
            const auto frequency = std::max(1.0f, static_cast<float>(k * sampleRate / FFT::size));
            const float decades = std::log10(frequency / 1000.0f);
            sensitivity[k] = decades < 0 ? 1 + 0.2f * std::max(-1.0f, decades)
                                        : 1 + 0.3f * std::min(1.0f, decades);
        }
        rate = sampleRate;
        setTiming(Tuning::attackMs, Tuning::releaseMs);
        reset();
    }
    void setTiming(float attackMs, float releaseMs, std::size_t interval = STFT::hop) noexcept
    {
        attack = std::exp(-static_cast<float>(interval / rate) / (std::max(1.0f, attackMs) * 0.001f));
        release = std::exp(-static_cast<float>(interval / rate) / (std::max(5.0f, releaseMs) * 0.001f));
    }
    void reset() noexcept { reduction.fill(0); }
    void analyzeMagnitude(const Spectrum& spectrum) noexcept
    {
        for (std::size_t k = 0; k < bins; ++k) magnitude[k] = std::abs(spectrum[k]);
    }
    void analyze(const Spectrum& spectrum) noexcept
    {
        float peak = 1.0e-12f;
        for (std::size_t k = 0; k < bins; ++k)
        {
            magnitude[k] = std::abs(spectrum[k]);
            peak = std::max(peak, magnitude[k]);
        }
        const float floor = peak * std::pow(10.0f, -Tuning::relativeFloorDb / 20);
        prefix[0] = 0;
        for (std::size_t k = 0; k < bins; ++k)
        {
            level[k] = 20 * std::log10(std::max(magnitude[k], floor));
            prefix[k + 1] = prefix[k] + level[k];
        }
        for (std::size_t k = 0; k < bins; ++k)
        {
            baseline[k] = static_cast<float>((prefix[high[k] + 1] - prefix[low[k]]) / (high[k] - low[k] + 1));
            resonance[k] = std::max(0.0f, level[k] - baseline[k] - Tuning::prominenceKneeDb);
            raw[k] = resonance[k] * Tuning::softRatio * sensitivity[k];
        }
    }
    void apply(Spectrum& spectrum, float depth) noexcept
    {
        analyze(spectrum);
        attenuate(spectrum, raw, depth);
    }
    void attenuate(Spectrum& spectrum, const Curve& target, float depth) noexcept
    {
        const float scale = std::clamp(depth, 0.0f, 10.0f) / Tuning::nominalDepth;
        for (std::size_t k = 0; k < bins; ++k)
        {
            const float wanted = std::clamp(target[k] * scale, 0.0f, Tuning::maximumReductionDb);
            const float coefficient = wanted > reduction[k] ? attack : release;
            reduction[k] = coefficient * reduction[k] + (1 - coefficient) * wanted;
            if (reduction[k] < 1.0e-15f) reduction[k] = 0;
            const float gain = std::pow(10.0f, -reduction[k] / 20);
            spectrum[k] *= gain;
            if (k > 0 && k < bins - 1) spectrum[FFT::size - k] *= gain;
        }
    }
    Curve magnitude{}, level{}, baseline{}, resonance{}, raw{}, reduction{};
private:
    Curve sensitivity{};
    std::array<double, bins + 1> prefix{};
    std::array<std::size_t, bins> low{}, high{};
    float attack = 0, release = 0;
    double rate = 48000;
};

}
