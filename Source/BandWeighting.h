#pragma once
#include "SoftDetector.h"

namespace soothe
{
enum class BandShape { bell, lowShelf, highShelf, lowCut, highCut };
struct Band
{
    bool enabled = true;
    BandShape shape = BandShape::bell;
    float frequency = 1000, amountDb = 0, widthOctaves = 1, slope = 24;
    float focus = 0; // -1: Left/Mid, +1: Right/Side
    bool operator==(const Band& b) const noexcept
    {
        return enabled == b.enabled && shape == b.shape && frequency == b.frequency
            && amountDb == b.amountDb && widthOctaves == b.widthOctaves && slope == b.slope && focus == b.focus;
    }
};
using Bands = std::array<Band, 5>;
inline Bands defaultBands() noexcept
{
    return {{{false, BandShape::lowCut, 80, 0, 1, 24},
             {true, BandShape::bell, 250, 0, 1, 24},
             {true, BandShape::bell, 1000, 0, 1, 24},
             {true, BandShape::bell, 4000, 0, 1, 24},
             {false, BandShape::highCut, 12000, 0, 1, 24}}};
}
// This curve scales reduction, never boosts the audio signal directly.
// Shared by DSP and editor so the drawn curve has the same definition as processing.
inline float bandWeightDb(float frequency, const Bands& bands) noexcept
{
    float total = 0;
    for (const auto& b : bands)
    {
        if (!b.enabled) continue;
        const float distance = std::log2(std::max(1.0f, frequency) / std::max(20.0f, b.frequency));
        const float width = std::max(0.15f, b.widthOctaves);
        switch (b.shape)
        {
            case BandShape::bell:
                total += b.amountDb * std::exp(-2.772588722f * distance * distance / (width * width)); break;
            case BandShape::lowShelf:
                total += b.amountDb / (1 + std::exp(std::clamp(4 * distance / width, -60.0f, 60.0f))); break;
            case BandShape::highShelf:
                total += b.amountDb / (1 + std::exp(std::clamp(-4 * distance / width, -60.0f, 60.0f))); break;
            case BandShape::lowCut:
            case BandShape::highCut:
            {
                const float signedDistance = b.shape == BandShape::lowCut ? -distance : distance;
                const float x = std::clamp(signedDistance * b.slope / 6.020599913f, -60.0f, 60.0f);
                total -= 20 * std::log10(1 + std::exp2(x)); break;
            }
        }
    }
    return std::clamp(total, -60.0f, 24.0f);
}
class BandWeighting
{
public:
    using Curve = SoftDetector::Curve;
    void prepare(double sampleRate) noexcept { rate = sampleRate; valid = false; }
    void apply(const Curve& input, const Bands& bands) noexcept
    {
        if (!valid || !(cached == bands))
        {
            cached = bands; valid = true;
            for (std::size_t k = 0; k < gain.size(); ++k)
                gain[k] = std::pow(10.0f, bandWeightDb(static_cast<float>(k * rate / FFT::size), bands) / 20);
        }
        for (std::size_t k = 0; k < gain.size(); ++k) result[k] = input[k] * gain[k];
    }
    Curve result{};
private:
    Curve gain{};
    Bands cached{};
    double rate = 48000;
    bool valid = false;
};


// Focus removes processing from the opposite component within this band's
// support. At centre it is neutral, including for a zero-dB bell.
inline float bandFocusGain(float frequency, const Bands& bands, int component) noexcept
{
    float gain = 1;
    for (const auto& b : bands)
    {
        if (!b.enabled || b.focus == 0) continue;
        const float d = std::log2(std::max(1.0f, frequency) / std::max(20.0f, b.frequency));
        const float w = std::max(0.15f, b.widthOctaves);
        float support = 0;
        switch (b.shape)
        {
            case BandShape::bell: support = std::exp(-2.772588722f * d * d / (w * w)); break;
            case BandShape::lowShelf: support = 1 / (1 + std::exp(std::clamp(4 * d / w, -60.0f, 60.0f))); break;
            case BandShape::highShelf: support = 1 / (1 + std::exp(std::clamp(-4 * d / w, -60.0f, 60.0f))); break;
            case BandShape::lowCut: support = 1 / (1 + std::exp2(std::clamp(-d * b.slope / 6.020599913f, -60.0f, 60.0f))); break;
            case BandShape::highCut: support = 1 / (1 + std::exp2(std::clamp(d * b.slope / 6.020599913f, -60.0f, 60.0f))); break;
        }
        const float opposite = component == 0 ? std::max(0.0f, b.focus) : std::max(0.0f, -b.focus);
        gain *= 1 - std::clamp(opposite, 0.0f, 1.0f) * support;
    }
    return gain;
}
}
