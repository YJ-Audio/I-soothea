#include "DetailProcessor.h"
#include <iostream>
#include <memory>

int main()
{
    auto stft = std::make_unique<soothe::STFT>();
    auto detector = std::make_unique<soothe::SoftDetector>();
    auto detail = std::make_unique<soothe::DetailProcessor>();
    detail->prepare();
    float previousWidth = 10000;
    float previousPeak = 0;
    for (float setting : {0.0f, 2.5f, 5.0f, 7.5f, 10.0f})
    {
        stft->reset(); detector->prepare(48000);
        for (int i = 0; i < 48000; ++i)
        {
            float sample = 0;
            for (double frequency : {500.0, 1000.0, 2000.0})
                sample += 0.15f * static_cast<float>(std::sin(6.283185307179586 * frequency * i / 48000));
            stft->tick(sample, [&](auto& spectrum) noexcept
            {
                detector->analyze(spectrum); detail->process(detector->raw, setting);
                detector->attenuate(spectrum, detail->result, 5);
            });
        }
        const auto& curve = detail->result;
        const auto bin = [](float frequency) { return static_cast<std::size_t>(std::round(frequency * 4096 / 48000)); };
        const float peak = curve[bin(1000)];
        const float valley = curve[bin(1414)];
        int width = 0;
        for (auto k = bin(1000); k > 0 && curve[k] > peak * 0.5f; --k) ++width;
        for (auto k = bin(1000) + 1; k < curve.size() && curve[k] > peak * 0.5f; ++k) ++width;
        std::cout << "Detail=" << setting << " peakDb=" << peak << " valley/peak=" << valley / peak << " halfWidthBins=" << width << '\n';
        if (width > previousWidth || peak < previousPeak) return 1;
        previousWidth = static_cast<float>(width); previousPeak = peak;
        if (setting == 0 && valley / peak < 0.5f) return 2;
        if (setting >= 5 && valley / peak > 0.5f) return 3;
        for (float frequency : {500.0f, 1000.0f, 2000.0f})
            if (curve[bin(frequency)] < 0.05f) return 4;
    }
}
