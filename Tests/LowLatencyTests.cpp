#include "StereoEngine.h"
#include <memory>
#include <iostream>
#include <crtdbg.h>
#include <limits>
static bool watching = false;
static int allocations = 0;
static int hook(int type, void*, std::size_t, int, long, const unsigned char*, int)
{ if (watching && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations; return 1; }
int main()
{
    _CrtSetAllocHook(hook);
    auto normal = std::make_unique<soothe::Engine>();
    auto low = std::make_unique<soothe::BasicEngine<2048>>();
    double worst = 0;
    for (bool quality : {false, true}) for (bool hard : {false, true}) for (double rate : {44100.0, 48000.0, 96000.0})
        for (double f : {100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0})
        {
            soothe::Settings s; s.hard = hard; s.highQuality = quality; normal->prepare(rate, s); low->prepare(rate, s);
            double a = 0, b = 0;
            watching = true;
            for (int i = 0; i < static_cast<int>(rate); ++i)
            {
                float in = static_cast<float>(0.3 * std::sin(6.283185307179586 * f * i / rate));
                float x = normal->tick(in), y = low->tick(in);
                if (!std::isfinite(x) || !std::isfinite(y)) return 1;
                if (i > rate / 2) { a += x * x; b += y * y; }
            }
            watching = false;
            if (normal->diagnostics().raw != low->diagnostics().raw || normal->diagnostics().reduction != low->diagnostics().reduction) return 2;
            const double diff = 10 * std::log10(a / b); worst = std::max(worst, std::abs(diff));
            std::cout << (hard ? "Hard" : "Soft") << " rate=" << rate << " f=" << f << " reductionDifferenceDb=" << diff << '\n';
        }
    std::cout << "maximum difference=" << worst << '\n';
    if (worst > 0.5) return 3;
    auto wet = std::make_unique<soothe::BasicStereoEngine<2048>>();
    auto removed = std::make_unique<soothe::BasicStereoEngine<2048>>();
    float neutralError = 0, deltaError = 0;
    for (double rate : {44100.0, 48000.0, 96000.0}) for (bool ms : {false, true})
    {
        soothe::Settings s; s.depth = 0; s.midSide = ms; wet->prepare(rate, s);
        watching = true;
        for (int i = 0; i < static_cast<int>(rate / 2); ++i)
        {
            if (i % 1703 == 0) { s.highQuality = !s.highQuality; wet->setSettings(s); }
            const float a = 0.2f * std::sin(i * 0.13f), b = 0.15f * std::sin(i * 0.07f);
            const auto out = wet->tick(a, b);
            const float x = i < 2048 ? 0 : 0.2f * std::sin((i - 2048) * 0.13f);
            const float y = i < 2048 ? 0 : 0.15f * std::sin((i - 2048) * 0.07f);
            neutralError = std::max({neutralError, std::abs(out[0] - x), std::abs(out[1] - y)});
        }
        watching = false;
        s.depth = 5; s.link = 0.7f; s.bands[2].focus = -0.7f;
        wet->prepare(rate, s); s.delta = true; removed->prepare(rate, s); s.delta = false;
        watching = true;
        for (int i = 0; i < static_cast<int>(rate / 2); ++i)
        {
            if (i % 4001 == 0)
            {
                s.highQuality = !s.highQuality; s.hard = !s.hard; s.midSide = !s.midSide;
                wet->setSettings(s); s.delta = true; removed->setSettings(s); s.delta = false;
            }
            const float a = 0.2f * std::sin(i * 0.13f), b = 0.15f * std::sin(i * 0.07f);
            const auto out = wet->tick(a, b), diff = removed->tick(a, b);
            const float x = i < 2048 ? 0 : 0.2f * std::sin((i - 2048) * 0.13f);
            const float y = i < 2048 ? 0 : 0.15f * std::sin((i - 2048) * 0.07f);
            if (!std::isfinite(out[0]) || !std::isfinite(out[1]) || std::abs(out[0]) > 1 || std::abs(out[1]) > 1) return 4;
            deltaError = std::max({deltaError, std::abs(out[0] + diff[0] - x), std::abs(out[1] + diff[1] - y)});
        }
        watching = false;
        for (int dryMode = 0; dryMode < 2; ++dryMode)
        {
            s = {}; s.bypass = dryMode == 0; s.mix = dryMode == 0 ? 1.0f : 0.0f; wet->prepare(rate, s);
            for (int i = 0; i < 6000; ++i)
            {
                const auto out = wet->tick(i == 0 ? 1.0f : 0.0f, 0);
                if (out[0] != (i == 2048 ? 1.0f : 0.0f) || out[1] != 0) return 5;
            }
        }
    }
    soothe::Settings silent; wet->prepare(48000, silent);
    for (int i = 0; i < 10000; ++i)
    {
        auto out = wet->tick(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
        if (out[0] != 0 || out[1] != 0) return 6;
    }
    std::cout << "Neutral error=" << neutralError << " Delta error=" << deltaError << " allocations=" << allocations << '\n';
    return neutralError < 2e-6f && deltaError < 2e-6f && allocations == 0 ? 0 : 7;
}
