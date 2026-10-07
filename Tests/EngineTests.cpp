#include "Engine.h"
#include <iostream>
#include <memory>
#include <limits>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static bool watching = false;
static int allocations = 0;
#ifdef _MSC_VER
static int allocationHook(int type, void*, std::size_t, int, long, const unsigned char*, int)
{
    if (watching && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations;
    return 1;
}
#endif

int main()
{
#ifdef _MSC_VER
    _CrtSetAllocHook(allocationHook);
#endif
    auto wet = std::make_unique<soothe::Engine>();
    auto delta = std::make_unique<soothe::Engine>();
    auto dry = std::make_unique<soothe::Engine>();
    soothe::Settings settings;
    double greatestNull = 0;
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        double lowFrequencyReduction = 0, midFrequencyReduction = 0;
        for (double frequency : {100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0})
        {
            double reference = 0;
            for (float level : {-6.0f, -12.0f, -24.0f, -36.0f})
            {
                settings = {}; wet->prepare(rate, settings);
                settings.delta = true; delta->prepare(rate, settings);
                settings.delta = false; settings.mix = 0; dry->prepare(rate, settings);
                const float amplitude = std::pow(10.0f, level / 20);
                double energyIn = 0, energyWet = 0, energyDelta = 0;
                watching = true;
                for (int i = 0; i < static_cast<int>(rate); ++i)
                {
                    const float sample = amplitude * static_cast<float>(std::sin(6.283185307179586 * frequency * i / rate));
                    const float w = wet->tick(sample), d = delta->tick(sample), a = dry->tick(sample);
                    if (!std::isfinite(w) || !std::isfinite(d) || std::abs(w) > 1.1f) return 1;
                    greatestNull = std::max(greatestNull, static_cast<double>(std::abs(a - w - d)));
                    if (i > rate / 2) { energyIn += a * a; energyWet += w * w; energyDelta += d * d; }
                }
                watching = false;
                const double reduction = 10 * std::log10(energyIn / energyWet);
                if (level == -6) reference = reduction;
                if (std::abs(reference - reduction) > 0.1 || reduction < 0.05 || energyDelta <= 0) return 2;
            }
            std::cout << "rate=" << rate << " frequency=" << frequency << " reductionDb=" << reference << '\n';
            if (frequency == 100) lowFrequencyReduction = reference;
            if (frequency == 1000) midFrequencyReduction = reference;
            if (frequency == 10000 && !(lowFrequencyReduction < midFrequencyReduction && midFrequencyReduction < reference)) return 5;
        }
    }
    settings = {}; wet->prepare(48000, settings);
    watching = true;
    for (int i = 0; i < 48000; ++i)
    {
        if (i % 127 == 0)
        {
            settings.depth = static_cast<float>(i % 11);
            settings.detail = static_cast<float>((i / 11) % 11);
            settings.delta = !settings.delta; settings.bypass = !settings.bypass;
            wet->setSettings(settings);
        }
        if (wet->tick(i == 10 ? std::numeric_limits<float>::quiet_NaN() : 0) != 0) return 3;
    }
    watching = false;
    std::cout << "Delta null max=" << greatestNull << " audio allocations=" << allocations << '\n';
    return greatestNull < 2.0e-7 && allocations == 0 ? 0 : 4;
}
