#include "StereoEngine.h"
#include <memory>
#include <iostream>
#include <crtdbg.h>
static bool watching = false;
static int allocations = 0;
static int hook(int type, void*, std::size_t, int, long, const unsigned char*, int)
{ if (watching && (type == _HOOK_ALLOC || type == _HOOK_REALLOC)) ++allocations; return 1; }
static float sample(int i, double rate, double frequency)
{ return 0.3f * static_cast<float>(std::sin(6.283185307179586 * frequency * i / rate)); }
int main()
{
    _CrtSetAllocHook(hook);
    auto normal = std::make_unique<soothe::Engine>();
    auto high = std::make_unique<soothe::Engine>();
    auto wet = std::make_unique<soothe::StereoEngine>();
    auto delta = std::make_unique<soothe::StereoEngine>();
    float maximumNeutral = 0, maximumDelta = 0;
    double maximumReductionDifference = 0;
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        // Arbitrary switching points must not break OLA normalization or latency.
        soothe::Settings s; s.depth = 0; wet->prepare(rate, s);
        watching = true;
        for (int i = 0; i < static_cast<int>(rate); ++i)
        {
            if (i % 701 == 0) { s.highQuality = !s.highQuality; wet->setSettings(s); }
            const auto out = wet->tick(sample(i, rate, 731), sample(i, rate, 3311));
            const float l = i < 4096 ? 0 : sample(i - 4096, rate, 731);
            const float r = i < 4096 ? 0 : sample(i - 4096, rate, 3311);
            maximumNeutral = std::max({maximumNeutral, std::abs(out[0] - l), std::abs(out[1] - r)});
        }
        watching = false;
        for (bool hard : {false, true}) for (double frequency : {100.0, 1000.0, 10000.0})
        {
            s = {}; s.hard = hard; normal->prepare(rate, s);
            s.highQuality = true; high->prepare(rate, s);
            double a = 0, b = 0;
            watching = true;
            for (int i = 0; i < static_cast<int>(rate); ++i)
            {
                const float in = sample(i, rate, frequency);
                const float n = normal->tick(in), h = high->tick(in);
                if (i > rate / 2) { a += n * n; b += h * h; }
            }
            watching = false;
            const double difference = std::abs(10 * std::log10(a / b));
            maximumReductionDifference = std::max(maximumReductionDifference, difference);
            if (difference > 0.15) { std::cerr << "Reduction differs " << rate << ' ' << frequency << ' ' << difference << '\n'; return 1; }
        }
        s = {}; s.link = 0.8f; wet->prepare(rate, s);
        s.delta = true; delta->prepare(rate, s); s.delta = false;
        watching = true;
        float previous = 0;
        for (int i = 0; i < static_cast<int>(rate); ++i)
        {
            if (i % 3503 == 0)
            {
                s.highQuality = !s.highQuality; s.midSide = !s.midSide;
                s.hard = !s.hard; s.bands[2].focus = s.highQuality ? -0.7f : 0.7f;
                wet->setSettings(s); s.delta = true; delta->setSettings(s); s.delta = false;
            }
            auto out = wet->tick(sample(i, rate, 1000), sample(i, rate, 2000));
            auto removed = delta->tick(sample(i, rate, 1000), sample(i, rate, 2000));
            if (!std::isfinite(out[0]) || !std::isfinite(out[1]) || std::abs(out[0]) > 1) return 2;
            if (i > 8192 && std::abs(out[0] - previous) > 0.12f) return 3;
            previous = out[0];
            for (int c = 0; c < 2; ++c)
            {
                const float aligned = i < 4096 ? 0 : sample(i - 4096, rate, c == 0 ? 1000 : 2000);
                maximumDelta = std::max(maximumDelta, std::abs(out[c] + removed[c] - aligned));
            }
        }
        watching = false;
    }
    // High really doubles update density; the AR time constant stays in seconds.
    soothe::STFT stft; stft.reset(512); int frames = 0;
    for (int i = 0; i < 8192; ++i) stft.tick(0, [&](auto&) { ++frames; });
    if (frames != 16) return 4;
    auto a = std::make_unique<soothe::SoftDetector>(), b = std::make_unique<soothe::SoftDetector>();
    a->prepare(48000); b->prepare(48000); a->setTiming(20, 120, 1024); b->setTiming(20, 120, 512);
    soothe::SoftDetector::Curve target{}; target.fill(10);
    auto spectrum = std::make_unique<soothe::FFT::Spectrum>();
    a->attenuate(*spectrum, target, 5); b->attenuate(*spectrum, target, 5); b->attenuate(*spectrum, target, 5);
    if (std::abs(a->reduction[100] - b->reduction[100]) > 1e-5f) return 5;
    target.fill(0);
    a->attenuate(*spectrum, target, 5); b->attenuate(*spectrum, target, 5); b->attenuate(*spectrum, target, 5);
    if (std::abs(a->reduction[100] - b->reduction[100]) > 1e-5f) return 6;
    std::cout << "Neutral switching=" << maximumNeutral << " Delta=" << maximumDelta
              << " steady reduction differenceDb=" << maximumReductionDifference << " allocations=" << allocations << '\n';
    return maximumNeutral < 2e-6f && maximumDelta < 2e-6f && allocations == 0 ? 0 : 7;
}
