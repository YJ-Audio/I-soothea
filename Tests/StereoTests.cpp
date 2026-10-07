#include "StereoEngine.h"
#include <memory>
#include <iostream>
#include <crtdbg.h>
#include <limits>
static bool watching = false;
static int allocations = 0;
static int hook(int type, void*, std::size_t, int, long, const unsigned char*, int)
{ if (watching && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations; return 1; }
static float signal(int i, double rate, double frequency = 1000)
{ return 0.3f * static_cast<float>(std::sin(6.283185307179586 * frequency * i / rate)); }
int main()
{
    _CrtSetAllocHook(hook);
    auto engine = std::make_unique<soothe::StereoEngine>();
    auto removed = std::make_unique<soothe::StereoEngine>();
    auto mono = std::make_unique<soothe::Engine>();
    float nullError = 0, legacyError = 0, identityError = 0;
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (bool hard : {false, true})
        {
            soothe::Settings s; s.hard = hard;
            engine->prepare(rate, s); mono->prepare(rate, s);
            watching = true;
            for (int i = 0; i < static_cast<int>(rate / 2); ++i)
            {
                const float in = signal(i, rate);
                auto out = engine->tick(in, in * 0.2f);
                legacyError = std::max(legacyError, std::abs(out[0] - mono->tick(in)));
            }
            watching = false;
            // Unity reconstruction even during repeated domain changes.
            s.depth = 0; engine->prepare(rate, s);
            watching = true;
            for (int i = 0; i < static_cast<int>(rate / 2); ++i)
            {
                if (i % 3072 == 0) { s.midSide = !s.midSide; engine->setSettings(s); }
                auto out = engine->tick(signal(i, rate), signal(i, rate, 1700));
                if (i >= 4096)
                {
                    identityError = std::max(identityError, std::abs(out[0] - signal(i - 4096, rate)));
                    identityError = std::max(identityError, std::abs(out[1] - signal(i - 4096, rate, 1700)));
                }
            }
            watching = false;
            s.depth = 5; s.link = 1; s.midSide = false;
            engine->prepare(rate, s); s.delta = true; removed->prepare(rate, s); s.delta = false;
            watching = true;
            for (int i = 0; i < static_cast<int>(rate); ++i)
            {
                if (i % 5120 == 0)
                {
                    s.midSide = !s.midSide; s.bands[2].focus = s.midSide ? -1.0f : 1.0f;
                    engine->setSettings(s); s.delta = true; removed->setSettings(s); s.delta = false;
                }
                const float l = signal(i, rate), r = signal(i, rate, 1700);
                auto out = engine->tick(l, r), diff = removed->tick(l, r);
                if (!std::isfinite(out[0]) || !std::isfinite(out[1])) return 1;
                if (i >= 4096)
                {
                    nullError = std::max(nullError, std::abs(out[0] + diff[0] - signal(i - 4096, rate)));
                    nullError = std::max(nullError, std::abs(out[1] + diff[1] - signal(i - 4096, rate, 1700)));
                }
            }
            watching = false;
        }
    if (legacyError > 1e-6f || identityError > 2e-6f || nullError > 2e-6f) return 2;
    // Same waveform at unequal levels: full link preserves the stereo ratio.
    soothe::Settings s; s.hard = true; s.link = 1;
    engine->prepare(48000, s);
    float ratioError = 0;
    for (int i = 0; i < 48000; ++i)
    {
        auto out = engine->tick(signal(i, 48000), signal(i, 48000) * 0.1f);
        ratioError = std::max(ratioError, std::abs(out[1] - out[0] * 0.1f));
    }
    if (ratioError > 2e-6f) return 3;
    // Focus audibly biases either component, including M/S encoded material.
    for (bool ms : {false, true})
        for (float focus : {-1.0f, 1.0f})
        {
            s.midSide = ms; s.bands[2].focus = focus; s.bands[2].widthOctaves = 4;
            engine->prepare(48000, s);
            double first = 0, second = 0;
            for (int i = 0; i < 48000; ++i)
            {
                const float a = signal(i, 48000);
                auto out = ms ? engine->tick(2 * a, 0) : engine->tick(a, a);
                if (i > 24000)
                {
                    const float x = ms ? (out[0] + out[1]) * 0.5f : out[0];
                    const float y = ms ? (out[0] - out[1]) * 0.5f : out[1];
                    first += x * x; second += y * y;
                }
            }
            const double balance = 10 * std::log10(first / second);
            std::cout << (ms ? "MS" : "LR") << " focus=" << focus << " balanceDb=" << balance << '\n';
            if (balance * focus < 3) return 4;
        }
    // All shapes are neutral at centre; disabled bands never affect focus.
    for (auto shape : {soothe::BandShape::bell, soothe::BandShape::lowShelf, soothe::BandShape::highShelf, soothe::BandShape::lowCut, soothe::BandShape::highCut})
    {
        auto bands = soothe::defaultBands(); bands[2].shape = shape;
        if (soothe::bandFocusGain(1000, bands, 0) != 1) return 5;
        bands[2].focus = 1;
        if (soothe::bandFocusGain(1000, bands, 0) >= 1 || soothe::bandFocusGain(1000, bands, 1) != 1) return 6;
        bands[2].enabled = false;
        if (soothe::bandFocusGain(1000, bands, 0) != 1) return 7;
    }
    // A silent component stays silent, including non-finite input sanitisation.
    s = {}; s.midSide = true; engine->prepare(48000, s);
    for (int i = 0; i < 10000; ++i)
    {
        auto out = engine->tick(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
        if (out[0] != 0 || out[1] != 0) return 8;
    }
    std::cout << "legacy=" << legacyError << " identity=" << identityError << " deltaNull=" << nullError
              << " linkedRatio=" << ratioError << " allocations=" << allocations << '\n';
    if (allocations != 0) return 9;
}
