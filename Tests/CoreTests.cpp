#include "STFT.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

int main()
{
    auto stft = std::make_unique<soothe::STFT>();
    std::mt19937 random(1979);
    std::uniform_real_distribution<float> noise(-0.8f, 0.8f);
    std::vector<float> input(96000), output(input.size() + soothe::STFT::latency);
    for (auto& sample : input) sample = noise(random);
    const std::size_t blocks[] = {1, 17, 64, 127, 511, 1024, 2049};
    float worst = 0;
    for (auto block : blocks)
    {
        stft->reset();
        for (std::size_t start = 0; start < output.size(); start += block)
            for (auto i = start; i < std::min(start + block, output.size()); ++i)
                output[i] = stft->tick(i < input.size() ? input[i] : 0, [](auto&) noexcept {});
        float error = 0;
        for (std::size_t i = 0; i < output.size(); ++i)
        {
            if (!std::isfinite(output[i])) return 1;
            const auto expected = i < soothe::STFT::latency ? 0 : input[i - soothe::STFT::latency];
            error = std::max(error, std::abs(output[i] - expected));
        }
        std::cout << "neutral block=" << block << " maxError=" << error << '\n';
        worst = std::max(worst, error);
    }
    stft->reset();
    for (int i = 0; i < 16000; ++i)
        if (stft->tick(0, [](auto&) noexcept {}) != 0) return 2;
    return worst < 2.0e-6f ? 0 : 3;
}
