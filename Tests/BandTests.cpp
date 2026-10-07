#include "Engine.h"
#include <memory>
#include <iostream>
#include <crtdbg.h>

static bool watching = false;
static int allocations = 0;
static int hook(int type, void*, std::size_t, int, long, const unsigned char*, int)
{ if (watching && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations; return 1; }
int main()
{
    _CrtSetAllocHook(hook);
    auto engine = std::make_unique<soothe::Engine>();
    const auto render = [&](soothe::Settings settings)
    {
        engine->prepare(48000, settings);
        std::array<double, 3> sine{}, cosine{}, amplitudes{};
        const double frequencies[] = {500, 1000, 2000};
        watching = true;
        for (int i = 0; i < 72000; ++i)
        {
            float sample = 0;
            for (double f : frequencies) sample += static_cast<float>(0.15 * std::sin(6.283185307179586 * f * i / 48000));
            const float output = engine->tick(sample);
            if (!std::isfinite(output)) std::abort();
            if (i >= 48000) for (int k = 0; k < 3; ++k)
            { sine[k] += output * std::sin(6.283185307179586 * frequencies[k] * i / 48000); cosine[k] += output * std::cos(6.283185307179586 * frequencies[k] * i / 48000); }
        }
        watching = false;
        for (int k = 0; k < 3; ++k) amplitudes[k] = 2 * std::hypot(sine[k], cosine[k]) / 24000;
        return amplitudes;
    };
    soothe::Settings settings;
    const auto neutral = render(settings);
    settings.bands[2].widthOctaves = 0.3f; settings.bands[2].amountDb = 12;
    const auto stronger = render(settings);
    const auto rawStrong = engine->diagnostics().raw;
    settings.bands[2].amountDb = -12;
    const auto weaker = render(settings);
    const auto rawWeak = engine->diagnostics().raw;
    if (rawStrong != rawWeak) return 1;
    if (!(stronger[1] < neutral[1] * 0.5 && weaker[1] > neutral[1] * 1.5)) return 2;
    for (int k : {0, 2}) if (std::abs(stronger[k] - neutral[k]) > 0.001 || std::abs(weaker[k] - neutral[k]) > 0.001) return 3;
    settings.bands[2].enabled = false;
    const auto disabled = render(settings);
    for (int k = 0; k < 3; ++k) if (std::abs(disabled[k] - neutral[k]) > 1.0e-6) return 4;
    settings.depth = 0; settings.bands[2].enabled = true; settings.bands[2].amountDb = 24;
    const auto depthZero = render(settings);
    for (auto amplitude : depthZero) if (std::abs(amplitude - 0.15) > 1.0e-5) return 5;
    auto bands = soothe::defaultBands();
    bands[2].shape = soothe::BandShape::lowShelf; bands[2].amountDb = 12;
    if (soothe::bandWeightDb(100, bands) < 11 || soothe::bandWeightDb(10000, bands) > 0.01) return 6;
    bands[2].shape = soothe::BandShape::highShelf;
    if (soothe::bandWeightDb(10000, bands) < 11 || soothe::bandWeightDb(100, bands) > 0.01) return 7;
    bands = soothe::defaultBands(); bands[0].enabled = true; bands[0].frequency = 500;
    if (soothe::bandWeightDb(100, bands) > -40 || soothe::bandWeightDb(10000, bands) < -0.01) return 8;
    bands[0].enabled = false; bands[4].enabled = true; bands[4].frequency = 2000;
    if (soothe::bandWeightDb(10000, bands) > -40 || soothe::bandWeightDb(100, bands) < -0.01) return 9;
    settings = {}; engine->prepare(48000, settings);
    float last = 0, step = 0;
    watching = true;
    for (int i = 0; i < 96000; ++i)
    {
        if (i % 127 == 0)
        {
            settings.bands[2].amountDb = i % 2 ? 24.0f : -24.0f;
            settings.bands[2].frequency = 300.0f + static_cast<float>(i % 4000);
            engine->setSettings(settings);
        }
        const float value = engine->tick(static_cast<float>(0.2 * std::sin(6.283185307179586 * i / 48)));
        if (!std::isfinite(value) || std::abs(value) > 1) return 10;
        if (i > 8192) step = std::max(step, std::abs(value - last));
        last = value;
    }
    watching = false;
    auto deltaEngine = std::make_unique<soothe::Engine>();
    settings = {}; settings.bands[2].amountDb = 9; settings.bands[0].enabled = true;
    engine->prepare(48000, settings); settings.delta = true; deltaEngine->prepare(48000, settings);
    double deltaError = 0;
    watching = true;
    for (int i = 0; i < 48000; ++i)
    {
        const float sample = static_cast<float>(0.2 * std::sin(6.283185307179586 * i / 48));
        const double dry = i < 4096 ? 0 : 0.2 * std::sin(6.283185307179586 * (i - 4096) / 48);
        deltaError = std::max(deltaError, std::abs(dry - engine->tick(sample) - deltaEngine->tick(sample)));
    }
    watching = false;
    std::cout << "1kHz amplitudes neutral/strong/weak=" << neutral[1] << '/' << stronger[1] << '/' << weaker[1]
              << " automation max step=" << step << " allocations=" << allocations << '\n';
    std::cout << "weighted Delta null=" << deltaError << '\n';
    return allocations == 0 && step < 0.05 && deltaError < 1.0e-6 ? 0 : 11;
}
