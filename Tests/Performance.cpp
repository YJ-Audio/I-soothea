#include "StereoEngine.h"
#include "LinearPhaseEngine.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>
#include <cstdlib>
#include <xmmintrin.h>
#include <pmmintrin.h>
int main(int argc, char** argv)
{
    using Clock = std::chrono::steady_clock;
    // Match juce::ScopedNoDenormals in the actual plugin callback.
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    const int rate = argc > 1 ? std::atoi(argv[1]) : 48000, block = argc > 2 ? std::atoi(argv[2]) : 256;
    if (rate < 8000 || rate > 192000 || block < 1 || block > 8192) return 1;
    const int blocks = rate * 10 / block;
    const double budgetUs = 1.0e6 * block / rate;
    std::cout << "rate=" << rate << " block=" << block << " budgetUs=" << budgetUs << std::endl;
    std::vector<float> input(blocks * block);
    for (int i = 0; i < static_cast<int>(input.size()); ++i)
        input[i] = static_cast<float>(0.15 * std::sin(i * 0.13) + 0.08 * std::sin(i * 0.043) + 0.03 * std::sin(i * 1.17));
    const bool normalOnly = argc > 3 && std::string(argv[3]) == "normal";
    double checksum = 0;
    auto measure = [&](auto& engine, const char* latencyName)
    {
    for (bool high : {false, true}) for (bool hard : {false, true}) for (bool ms : {false, true})
    {
        if (normalOnly && high) continue;
        soothe::Settings s; s.hard = hard; s.midSide = ms; s.link = 0.75f; s.highQuality = high;
        engine.prepare(rate, s);
        std::vector<double> times(blocks);
        for (int b = 0; b < blocks; ++b)
        {
            const auto start = Clock::now();
            engine.setSettings(s);
            for (int i = 0; i < block; ++i)
            {
                const int k = b * block + i;
                auto out = engine.tick(input[k], input[(k + 71) % input.size()] * 0.7f);
                checksum += out[0] + out[1];
            }
            times[b] = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        }
        double total = 0; for (auto time : times) total += time;
        std::sort(times.begin(), times.end());
        std::cout << latencyName << " " << (high ? "High " : "Normal ") << (hard ? "Hard" : "Soft") << (ms ? " MS" : " LR")
                  << " avgRealtimePercent=" << 100 * total / (blocks * budgetUs)
                  << " p99RealtimePercent=" << 100 * times[blocks * 99 / 100] / budgetUs << " overBudgetBlocks=" << std::count_if(times.begin(), times.end(), [&](double t) { return t > budgetUs; }) << " p99BlockUs=" << times[blocks * 99 / 100] << " maxBlockUs=" << times.back() << '\n';
    }
    };
    auto normal = std::make_unique<soothe::StereoEngine>(); measure(*normal, "StandardLatency");
    if (!normalOnly) {
    auto low = std::make_unique<soothe::BasicStereoEngine<2048>>(); measure(*low, "LowLatency");
    auto linear = std::make_unique<soothe::LinearPhaseEngine>(); measure(*linear, "LinearPhase");
    }
    std::cout << "checksum=" << checksum << '\n';
}
