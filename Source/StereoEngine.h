#pragma once
#include "Engine.h"

namespace soothe
{
// Both domains stay warm. Mode changes blend their reconstructed L/R spectra,
// so previously queued overlap-add samples never change channel meaning.
template <std::size_t N>
class BasicStereoEngine
{
public:
    using STFT = BasicSTFT<4096, N>;
    using Spectrum = FFT::Spectrum;
    using Curve = SoftDetector::Curve;
    void prepare(double sampleRate, const Settings& settings = {}) noexcept
    {
        rate = sampleRate;
        for (auto& s : stft) s.reset(settings.highQuality ? STFT::hop / 2 : STFT::hop);
        for (auto& c : chains)
        {
            c.detector.prepare(rate); c.hard.prepare(rate);
            c.detail.prepare(); c.weight.prepare(rate);
        }
        for (auto& d : dry) d.fill(0);
        position = 0; focusValid = false;
        coefficient = std::exp(-1.0f / static_cast<float>(0.005 * rate));
        setSettings(settings);
        mix = target.mix; gain = targetGain;
        delta = target.delta ? 1.0f : 0.0f; bypass = target.bypass ? 1.0f : 0.0f;
        modeBlend = target.midSide ? 1.0f : 0.0f;
    }
    void setSettings(const Settings& settings) noexcept
    {
        target = settings;
        targetGain = std::pow(10.0f, std::clamp(target.outputDb, -24.0f, 12.0f) / 20);
        for (auto& s : stft) s.setHighQuality(target.highQuality);
    }
    std::array<float, 2> tick(float left, float right) noexcept
    {
        const std::array<float, 2> input{{std::isfinite(left) ? left : 0, std::isfinite(right) ? right : 0}};
        std::array<float, 2> aligned{}, wet{}, output{};
        for (int c = 0; c < 2; ++c)
        {
            aligned[c] = dry[c][position]; dry[c][position] = input[c];
            wet[c] = stft[c].push(input[c]);
        }
        position = (position + 1) % STFT::latency;
        if (stft[0].frameReady()) processFrame();
        smooth(mix, std::clamp(target.mix, 0.0f, 1.0f)); smooth(gain, targetGain);
        smooth(delta, target.delta ? 1.0f : 0.0f); smooth(bypass, target.bypass ? 1.0f : 0.0f);
        for (int c = 0; c < 2; ++c)
        {
            const float normal = aligned[c] + mix * (wet[c] - aligned[c]);
            const float monitored = normal + delta * ((aligned[c] - wet[c]) - normal);
            output[c] = (1 - bypass) * monitored * gain + bypass * aligned[c];
        }
        return output;
    }
    const SoftDetector& diagnostics(int component) const noexcept
    { return chains[(target.midSide ? 2 : 0) + component].detector; }
private:
    struct Chain
    {
        SoftDetector detector;
        HardDetector hard;
        DetailProcessor detail;
        BandWeighting weight;
        Curve wanted{};
    };
    void processFrame() noexcept
    {
        const auto interval = stft[0].frameInterval();
        for (auto& c : chains) c.detector.setTiming(target.attack, target.release, interval);
        modeCoefficient = std::exp(-static_cast<float>(interval / rate) / 0.03f);
        if (!focusValid || !(cachedBands == target.bands))
        {
            cachedBands = target.bands; focusValid = true;
            for (int c = 0; c < 2; ++c)
                for (std::size_t k = 0; k < SoftDetector::bins; ++k)
                    focus[c][k] = bandFocusGain(static_cast<float>(k * rate / FFT::size), target.bands, c);
        }
        work[0] = stft[0].frame(); work[1] = stft[1].frame();
        // Half-sum convention: centred dual-mono stays at the same Hard level.
        for (std::size_t k = 0; k < FFT::size; ++k)
        {
            work[2][k] = (work[0][k] + work[1][k]) * 0.5f;
            work[3][k] = (work[0][k] - work[1][k]) * 0.5f;
        }
        for (int c = 0; c < 4; ++c)
        {
            auto& chain = chains[c];
            if (target.hard)
            {
                chain.detector.analyzeMagnitude(work[c]);
                chain.hard.analyze(chain.detector.magnitude);
            }
            else chain.detector.analyze(work[c]);
            chain.detail.process(target.hard ? chain.hard.raw : chain.detector.raw, target.detail);
            chain.weight.apply(chain.detail.result, target.bands);
        }
        const float link = std::clamp(target.link, 0.0f, 1.0f);
        for (int base : {0, 2})
        {
            for (std::size_t k = 0; k < SoftDetector::bins; ++k)
            {
                const float a = chains[base].weight.result[k], b = chains[base + 1].weight.result[k];
                const float shared = std::max(a, b);
                chains[base].wanted[k] = (a + link * (shared - a)) * focus[0][k];
                chains[base + 1].wanted[k] = (b + link * (shared - b)) * focus[1][k];
            }
            for (int c = base; c < base + 2; ++c)
                chains[c].detector.attenuate(work[c], chains[c].wanted, target.depth);
        }
        modeBlend = (target.midSide ? 1.0f : 0.0f) + modeCoefficient * (modeBlend - (target.midSide ? 1.0f : 0.0f));
        for (std::size_t k = 0; k < FFT::size; ++k)
        {
            stft[0].frame()[k] = work[0][k] + modeBlend * ((work[2][k] + work[3][k]) - work[0][k]);
            stft[1].frame()[k] = work[1][k] + modeBlend * ((work[2][k] - work[3][k]) - work[1][k]);
        }
        for (auto& s : stft) s.synthesise();
    }
    void smooth(float& current, float wanted) const noexcept
    {
        current = wanted + coefficient * (current - wanted);
        if (std::abs(current - wanted) < 1.0e-6f) current = wanted;
    }
    std::array<STFT, 2> stft;
    std::array<Chain, 4> chains;
    std::array<Spectrum, 4> work{};
    std::array<Curve, 2> focus{};
    std::array<std::array<float, STFT::latency>, 2> dry{};
    Bands cachedBands{};
    Settings target;
    double rate = 48000;
    std::size_t position = 0;
    bool focusValid = false;
    float coefficient = 0, modeCoefficient = 0, modeBlend = 0;
    float mix = 1, gain = 1, targetGain = 1, delta = 0, bypass = 0;
};
using StereoEngine = BasicStereoEngine<4096>;
}
