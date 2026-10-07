#include "Engine.h"
#include <crtdbg.h>
#include <iostream>
#include <memory>

static bool watching = false;
static int allocations = 0;
static int hook(int type, void*, std::size_t, int, long, const unsigned char*, int)
{ if (watching && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations; return 1; }
int main()
{
    _CrtSetAllocHook(hook);
    auto engine = std::make_unique<soothe::Engine>();
    auto delta = std::make_unique<soothe::Engine>();
    const auto render = [&](double rate, double frequency, float level, soothe::Settings settings)
    {
        engine->prepare(rate, settings);
        const float amplitude = std::pow(10.0f, level / 20);
        double inEnergy = 0, outEnergy = 0;
        watching = true;
        const int length = static_cast<int>(rate);
        for (int i = 0; i < length; ++i)
        {
            const float input = amplitude * static_cast<float>(std::sin(6.283185307179586 * frequency * i / rate));
            const float output = engine->tick(input);
            if (!std::isfinite(output) || std::abs(output) > 1.5f) std::abort();
            if (i >= length / 2)
            {
                const double aligned = amplitude * std::sin(6.283185307179586 * frequency * (i - 4096) / rate);
                inEnergy += aligned * aligned; outEnergy += output * output;
            }
        }
        watching = false;
        return 10 * std::log10(inEnergy / outEnergy);
    };
    soothe::Settings settings; settings.hard = true;
    const double frequencies[] = {100, 200, 500, 1000, 2000, 5000, 10000};
    const float measuredLevels[] = {0, -4.5f, -13, -17.5f, -24.5f, -38, -38};
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        for (int f = 0; f < 7; ++f)
        {
            const double r = render(rate, frequencies[f], measuredLevels[f], settings);
            std::cout << "reference rate=" << rate << " frequency=" << frequencies[f] << " input=" << measuredLevels[f] << " reduction=" << r << '\n';
            if (r < 1.5 || r > 4.5) return 1;
            double previous = 100;
            for (float level : {-6.0f, -12.0f, -24.0f, -36.0f})
            {
                const double value = render(rate, frequencies[f], level, settings);
                if (value > previous + 0.001 || value < -0.001) return 2;
                previous = value;
                if (rate == 48000 && f == 3) std::cout << "1kHz input=" << level << " reduction=" << value << '\n';
            }
        }
    }
    // Tunable transfer curve and log-frequency interpolation: exact control points.
    for (const auto& point : soothe::HardTuning::sensitivity)
        if (std::abs(soothe::HardDetector::frequencyWeightDb(point[0]) - point[1]) > 1.0e-5f) return 3;
    if (std::abs(soothe::HardDetector::frequencyWeightDb(std::sqrt(100.0f * 200)) - 2.25f) > 1.0e-5f) return 4;
    if (soothe::HardDetector::transfer(10, 1, 6) != 0 || soothe::HardDetector::transfer(-10) != 0) return 5;
    if (std::abs(soothe::HardDetector::transfer(10, 2, 0) - 5) > 1.0e-5f) return 6;
    // Final pipeline Depth and Detail remain monotonic at the isolated resonance.
    double previous = -1;
    for (float depth : {0.0f, 2.5f, 5.0f, 10.0f})
    {
        settings.depth = depth;
        const double r = render(48000, 1000, -12, settings);
        if (r < previous - 0.001 || (depth == 0 && std::abs(r) > 0.001)) return 7;
        previous = r;
    }
    settings.depth = 5; previous = -1;
    for (float detail : {0.0f, 2.5f, 5.0f, 7.5f, 10.0f})
    {
        settings.detail = detail;
        const double r = render(48000, 1000, -12, settings);
        if (r < previous - 0.001) return 8;
        previous = r;
    }
    settings.detail = 5;
    const auto normal = render(48000, 1000, -12, settings);
    settings.bands[2].amountDb = 6;
    if (render(48000, 1000, -12, settings) < normal * 1.5) return 9;
    // Mode switching retains a single reduction envelope and aligned Delta.
    engine->prepare(48000, settings); settings.delta = true; delta->prepare(48000, settings);
    double nullError = 0; float previousSample = 0, step = 0;
    watching = true;
    for (int i = 0; i < 96000; ++i)
    {
        if (i % 4001 == 0)
        {
            settings.hard = !settings.hard;
            settings.delta = false; engine->setSettings(settings);
            settings.delta = true; delta->setSettings(settings);
        }
        const float input = static_cast<float>(0.2 * std::sin(6.283185307179586 * i / 48));
        const float output = engine->tick(input), removed = delta->tick(input);
        const double dry = i < 4096 ? 0 : 0.2 * std::sin(6.283185307179586 * (i - 4096) / 48);
        nullError = std::max(nullError, std::abs(dry - output - removed));
        if (i > 8192) step = std::max(step, std::abs(output - previousSample));
        previousSample = output;
    }
    watching = false;
    settings.hard = true; engine->prepare(48000, settings);
    watching = true;
    for (int i = 0; i < 16000; ++i) if (engine->tick(0) != 0) return 10;
    watching = false;
    std::cout << "Hard/switch Delta null=" << nullError << " max step=" << step << " allocations=" << allocations << '\n';
    return allocations == 0 && nullError < 1.0e-6 && step < 0.04 ? 0 : 11;
}
