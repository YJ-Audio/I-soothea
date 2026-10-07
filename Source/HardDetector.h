#pragma once
#include "SoftDetector.h"

namespace soothe
{
struct HardTuning
{
    // Effective model of supplied measurements, not Soothe3 implementation facts.
    static constexpr float thresholdDb = -5.0f;
    static constexpr float ratio = 2.0f;
    static constexpr float kneeDb = 6.0f;
    static constexpr std::array<std::array<float, 2>, 7> sensitivity{{
        {{100, 0}}, {{200, 4.5f}}, {{500, 13}}, {{1000, 17.5f}},
        {{2000, 24.5f}}, {{5000, 38}}, {{10000, 38}}
    }};
};

class HardDetector
{
public:
    using Curve = SoftDetector::Curve;
    static float frequencyWeightDb(float frequency) noexcept
    {
        const auto& points = HardTuning::sensitivity;
        if (frequency <= points.front()[0]) return points.front()[1];
        for (std::size_t i = 1; i < points.size(); ++i)
            if (frequency < points[i][0])
            {
                const float t = std::log(frequency / points[i - 1][0]) / std::log(points[i][0] / points[i - 1][0]);
                return points[i - 1][1] + t * (points[i][1] - points[i - 1][1]);
            }
        return points.back()[1];
    }
    static float transfer(float excessDb, float ratio = HardTuning::ratio, float knee = HardTuning::kneeDb) noexcept
    {
        const float slope = 1 - 1 / std::max(1.0f, ratio);
        knee = std::max(0.0f, knee);
        if (excessDb <= -knee / 2) return 0;
        if (knee == 0 || excessDb >= knee / 2) return slope * std::max(0.0f, excessDb);
        const float offset = excessDb + knee / 2;
        return slope * offset * offset / (2 * knee);
    }
    void prepare(double sampleRate) noexcept
    {
        for (std::size_t k = 0; k < sensitivity.size(); ++k)
            sensitivity[k] = frequencyWeightDb(static_cast<float>(k * sampleRate / FFT::size));
        raw.fill(0); levelDb.fill(-240);
    }
    void analyze(const Curve& magnitude) noexcept
    {
        for (std::size_t k = 0; k < raw.size(); ++k)
        {
            // Periodic Hann sum=N/2. A bin-centred real sine has |X|=A*N/4;
            // DC and Nyquist are unpaired and therefore use N/2 instead.
            const float divisor = k == 0 || k == raw.size() - 1 ? FFT::size / 2.0f : FFT::size / 4.0f;
            levelDb[k] = 20 * std::log10(std::max(1.0e-12f, magnitude[k] / divisor));
            raw[k] = transfer(levelDb[k] + sensitivity[k] - HardTuning::thresholdDb);
        }
    }
    Curve raw{}, levelDb{};
private:
    Curve sensitivity{};
};

}
