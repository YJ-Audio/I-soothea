#include "SoftDetector.h"
#include <iostream>
#include <memory>

int main()
{
    auto stft = std::make_unique<soothe::STFT>();
    auto detector = std::make_unique<soothe::SoftDetector>();
    float reference = 0;
    float last = -1;
    for (float depth : {0.0f, 2.5f, 5.0f, 10.0f})
    {
        for (float level : {-6.0f, -12.0f, -24.0f, -36.0f})
        {
            stft->reset(); detector->prepare(48000);
            double dryEnergy = 0, wetEnergy = 0;
            const float amplitude = std::pow(10.0f, level / 20);
            for (int i = 0; i < 96000; ++i)
            {
                const float input = amplitude * static_cast<float>(std::sin(6.283185307179586 * 1000 * i / 48000));
                const float output = stft->tick(input, [&](auto& spectrum) noexcept { detector->apply(spectrum, depth); });
                if (!std::isfinite(output) || std::abs(output) > 1) return 1;
                if (i >= 48000)
                {
                    dryEnergy += input * input;
                    wetEnergy += output * output;
                }
            }
            const float reduction = static_cast<float>(10 * std::log10(dryEnergy / wetEnergy));
            std::cout << "depth=" << depth << " inputDb=" << level << " reductionDb=" << reduction << '\n';
            if (level == -6) { reference = reduction; if (reduction < last - 0.001f) return 2; last = reduction; }
            if (std::abs(reference - reduction) > 0.05f) return 3;
            if (depth == 0 && std::abs(reduction) > 0.001f) return 4;
            if (depth == 5 && (reduction < 1 || detector->reduction[85] <= detector->reduction[170] + 1)) return 5;
        }
    }
}
