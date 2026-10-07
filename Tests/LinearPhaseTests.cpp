#include "LinearPhaseEngine.h"
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
    auto linear = std::make_unique<soothe::LinearPhaseEngine>();
    auto normal = std::make_unique<soothe::Engine>();
    auto delta = std::make_unique<soothe::LinearPhaseEngine>();
    auto transform = std::make_unique<soothe::LinearPhaseDesigner::Transform>();
    auto designer = std::make_unique<soothe::LinearPhaseDesigner>();
    auto response = std::make_unique<soothe::LinearPhaseDesigner::Spectrum>();
    soothe::SoftDetector::Curve amplitude{};
    for (std::size_t k = 0; k < amplitude.size(); ++k)
        amplitude[k] = 1 - 0.8f * std::exp(-std::pow((static_cast<float>(k) - 250) / 50, 2));
    designer->design(amplitude, *transform, *response);
    float phaseError = 0;
    for (std::size_t k = 0; k <= 4096; ++k)
    {
        // Remove the claimed fixed group delay; the response must be real-positive.
        const auto rotated = (*response)[k] * std::polar(1.0f, static_cast<float>(6.283185307179586 * (k % 4) / 4));
        phaseError = std::max(phaseError, std::abs(rotated.imag()));
        if (rotated.real() < 0.19f) return 1;
    }
    transform->transform(*response, true);
    for (std::size_t k = 0; k <= 4096; ++k)
        if (std::abs((*response)[k].real() - (*response)[4096-k].real()) > 1e-6f) return 2;
    if (phaseError > 2e-6f) return 3;
    auto second = amplitude; std::reverse(second.begin(), second.end());
    auto independentA = std::make_unique<soothe::LinearPhaseDesigner::Spectrum>();
    auto independentB = std::make_unique<soothe::LinearPhaseDesigner::Spectrum>();
    auto packedA = std::make_unique<soothe::LinearPhaseDesigner::Spectrum>();
    auto packedB = std::make_unique<soothe::LinearPhaseDesigner::Spectrum>();
    designer->design(amplitude, *transform, *independentA); designer->design(second, *transform, *independentB);
    designer->designPair(amplitude, second, *transform, *packedA, *packedB);
    for (std::size_t k = 0; k < 8192; ++k)
        if (std::abs((*packedA)[k] - (*independentA)[k]) > 2e-6f || std::abs((*packedB)[k] - (*independentB)[k]) > 2e-6f) return 11;
    double worst = 0, worstTonePhase = 0;
    for (bool hard : {false, true}) for (double rate : {44100.0, 48000.0, 96000.0})
        for (double frequency : {100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0})
        {
            soothe::Settings s; s.hard = hard; linear->prepare(rate, s, true); normal->prepare(rate, s);
            double a = 0, b = 0, ss = 0, cc = 0, sc = 0, ys = 0, yc = 0;
            watching = true;
            for (int i = 0; i < static_cast<int>(rate); ++i)
            {
                const float in = static_cast<float>(0.3 * std::sin(6.283185307179586 * frequency * i / rate));
                const float x = normal->tick(in), y = linear->tick(in, 0)[0];
                if (!std::isfinite(y)) return 4;
                                            if (i > rate / 2)
                {
                    a += x*x; b += y*y;
                    const double angle = 6.283185307179586 * frequency * (i - 3072) / rate;
                    const double sine = std::sin(angle), cosine = std::cos(angle);
                    ss += sine*sine; cc += cosine*cosine; sc += sine*cosine; ys += y*sine; yc += y*cosine;
                }
            }
            watching = false;
            if (normal->diagnostics().reduction != linear->diagnostics(0).reduction) return 5;
            const double tonePhase = std::atan2(yc*ss - ys*sc, ys*cc - yc*sc); worstTonePhase = std::max(worstTonePhase, std::abs(tonePhase));
            const double difference = 10 * std::log10(a / b); worst = std::max(worst, std::abs(difference));
            std::cout << (hard ? "Hard" : "Soft") << " rate=" << rate << " f=" << frequency << " differenceDb=" << difference << '\n';
        }
    std::cout << "phaseError=" << phaseError << " maximum reduction difference=" << worst << " allocations=" << allocations << '\n';
    if (worst > 0.5 || worstTonePhase > 0.005) return 6;
    float neutralError = 0, deltaError = 0, maximumStep = 0;
    for (double rate : {44100.0, 48000.0, 96000.0}) for (bool ms : {false, true})
    {
        soothe::Settings s; s.depth = 0; s.midSide = ms; linear->prepare(rate, s);
        watching = true;
        for (int i = 0; i < static_cast<int>(rate / 2); ++i)
        {
            if (i % 1703 == 0) { s.highQuality = !s.highQuality; linear->setSettings(s); }
            const auto out = linear->tick(0.2f * std::sin(i * 0.13f), 0.15f * std::sin(i * 0.07f));
            const float a = i < 3072 ? 0 : 0.2f * std::sin((i-3072) * 0.13f);
            const float b = i < 3072 ? 0 : 0.15f * std::sin((i-3072) * 0.07f);
            neutralError = std::max({neutralError, std::abs(out[0]-a), std::abs(out[1]-b)});
        }
        watching = false;
        s.depth = 5; s.link = 0.7f; s.bands[2].focus = -0.7f;
        linear->prepare(rate, s); s.delta = true; delta->prepare(rate, s); s.delta = false;
        float previous = 0;
        watching = true;
        for (int i = 0; i < static_cast<int>(rate / 2); ++i)
        {
            if (i % 4001 == 0)
            {
                s.highQuality = !s.highQuality; s.hard = !s.hard; s.midSide = !s.midSide;
                s.bands[2].focus = -s.bands[2].focus;
                linear->setSettings(s); s.delta = true; delta->setSettings(s); s.delta = false;
            }
            const float l = 0.2f * std::sin(i * 0.13f), r = 0.15f * std::sin(i * 0.07f);
            const auto out = linear->tick(l, r), diff = delta->tick(l, r);
            if (!std::isfinite(out[0]) || !std::isfinite(out[1])) return 7;
            const float a = i < 3072 ? 0 : 0.2f * std::sin((i-3072) * 0.13f);
            const float b = i < 3072 ? 0 : 0.15f * std::sin((i-3072) * 0.07f);
            deltaError = std::max({deltaError, std::abs(out[0]+diff[0]-a), std::abs(out[1]+diff[1]-b)});
            maximumStep = std::max(maximumStep, std::abs(out[0] - previous)); previous = out[0];
        }
        watching = false;
        for (int dryMode = 0; dryMode < 2; ++dryMode)
        {
            s = {}; s.bypass = dryMode == 0; s.mix = dryMode == 0 ? 1.0f : 0.0f; linear->prepare(rate, s);
            for (int i = 0; i < 8000; ++i)
            {
                const auto out = linear->tick(i == 0 ? 1.0f : 0.0f, 0);
                if (out[0] != (i == 3072 ? 1.0f : 0.0f) || out[1] != 0) return 8;
            }
        }
    }
    for (bool ms : {false, true}) for (float focus : {-1.0f, 1.0f})
    {
        soothe::Settings s; s.hard = true; s.midSide = ms; s.link = 1; s.bands[2].focus = focus; s.bands[2].widthOctaves = 4;
        linear->prepare(48000, s); double a = 0, b = 0;
        for (int i = 0; i < 48000; ++i)
        {
            const float in = 0.3f * static_cast<float>(std::sin(6.283185307179586 * i / 48));
            const auto out = ms ? linear->tick(2*in, 0) : linear->tick(in, in);
            if (i > 24000)
            {
                const float x = ms ? (out[0] + out[1])*0.5f : out[0], y = ms ? (out[0] - out[1])*0.5f : out[1];
                a += x*x; b += y*y;
            }
        }
        if (10*std::log10(a/b)*focus < 3) return 12;
    }
    soothe::Settings silent; linear->prepare(48000, silent);
    for (int i = 0; i < 12000; ++i)
    {
        auto out = linear->tick(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity());
        if (out[0] != 0 || out[1] != 0) return 9;
    }
    std::cout << "tone phase radians=" << worstTonePhase << " neutral=" << neutralError << " delta=" << deltaError
              << " maxStep=" << maximumStep << " allocations=" << allocations << '\n';
    return allocations == 0 && neutralError < 2e-6f && deltaError < 2e-6f && maximumStep < 0.08f ? 0 : 10;
}
