#pragma once
#include "Engine.h"

namespace soothe
{
// Symmetric, causal FIR; its frozen response has a 2048-sample group delay.
// Dynamic coefficient interpolation preserves tap symmetry, but is not an LTI system.
class LinearPhaseDesigner
{
public:
    using Transform = BasicFFT<8192>;
    using Spectrum = Transform::Spectrum;
    static constexpr std::size_t groupDelay = 2048, taps = 4097;
    LinearPhaseDesigner()
    {
        for (std::size_t i = 0; i <= groupDelay; ++i)
            window[i] = static_cast<float>(0.5 + 0.5 * std::cos(6.283185307179586 * i / (taps - 1)));
    }
    void design(const SoftDetector::Curve& amplitude, Transform& fft, Spectrum& response) noexcept
    {
        makeImpulse(amplitude, response);
        fft.transform(response, false);
    }
    void designPair(const SoftDetector::Curve& a, const SoftDetector::Curve& b,
                    Transform& fft, Spectrum& left, Spectrum& right) noexcept
    {
        makeImpulse(a, left); makeImpulse(b, right);
        for (std::size_t k = 0; k < Transform::size; ++k) left[k] = {left[k].real(), right[k].real()};
        fft.transform(left, false);
        // One complex FFT carries two real FIR responses.
        for (std::size_t k = 0; k <= Transform::size / 2; ++k)
        {
            const auto mirror = (Transform::size - k) % Transform::size;
            const auto first = left[k], second = std::conj(left[mirror]);
            const auto l = (first + second) * 0.5f, r = (first - second) * std::complex<float>(0, -0.5f);
            left[k] = l; right[k] = r;
            if (mirror != k) { left[mirror] = std::conj(l); right[mirror] = std::conj(r); }
        }
    }
private:
    void makeImpulse(const SoftDetector::Curve& amplitude, Spectrum& response) noexcept
    {
        for (std::size_t k = 0; k < amplitude.size(); ++k)
        {
            impulseSpectrum[k] = amplitude[k];
            if (k > 0 && k < FFT::size / 2) impulseSpectrum[FFT::size - k] = amplitude[k];
        }
        inverse.transform(impulseSpectrum, true);
        for (std::size_t lag = 0; lag <= groupDelay; ++lag)
        {
            const float value = (lag == 0 ? impulseSpectrum[0].real()
                : 0.5f * (impulseSpectrum[lag].real() + impulseSpectrum[FFT::size - lag].real())) * window[lag];
            impulse[groupDelay - lag] = value; impulse[groupDelay + lag] = value;
        }
        response.fill({});
        for (std::size_t k = 0; k < taps; ++k) response[k] = impulse[k];
    }
    FFT inverse;
    FFT::Spectrum impulseSpectrum{};
    std::array<float, groupDelay + 1> window{};
    std::array<float, taps> impulse{};
};

class LinearPhaseEngine
{
public:
    static constexpr std::size_t latency = 3072, maximumHop = 1024;
    using Transform = LinearPhaseDesigner::Transform;
    using Spectrum = Transform::Spectrum;
    using Curve = SoftDetector::Curve;
    void prepare(double sampleRate, const Settings& settings = {}, bool mono = false) noexcept
    {
        rate = sampleRate; monoInput = mono;
        for (auto& c : chains)
        {
            c.detector.prepare(rate); c.hard.prepare(rate); c.detail.prepare(); c.weight.prepare(rate);
        }
        for (std::size_t i = 0; i < FFT::size; ++i)
            analysisWindow[i] = static_cast<float>(0.5 - 0.5 * std::cos(6.283185307179586 * i / FFT::size));
        for (auto& x : input) x.fill(0);
        for (auto& x : output) x.fill(0);
        for (auto& x : dry) x.fill(0);
        position = outputPosition = dryPosition = counter = 0; focusValid = false;
        setSettings(settings);
        interval = target.highQuality ? 512 : maximumHop;
        coefficient = std::exp(-1.0f / static_cast<float>(0.005 * rate));
        modeBlend = target.midSide ? 1.0f : 0.0f;
        mix = target.mix; gain = targetGain; delta = target.delta ? 1.0f : 0.0f; bypass = target.bypass ? 1.0f : 0.0f;
        for (int c = 0; c < 3; ++c)
        {
            matrix[c].fill(c == 2 ? 0.0f : 1.0f);
            designer.design(matrix[c], fft, previous[c]);
        }
    }
    void setSettings(const Settings& settings) noexcept
    {
        target = settings;
        if (monoInput)
        {
            target.midSide = false; target.link = 0;
            for (auto& band : target.bands) band.focus = 0;
        }
        targetGain = std::pow(10.0f, std::clamp(target.outputDb, -24.0f, 12.0f) / 20);
    }
    std::array<float, 2> tick(float left, float right) noexcept
    {
        const std::array<float, 2> in{{std::isfinite(left) ? left : 0, std::isfinite(right) ? right : 0}};
        std::array<float, 2> wet{}, aligned{}, result{};
        for (int c = 0; c < 2; ++c)
        {
            input[c][position] = in[c]; wet[c] = output[c][outputPosition]; output[c][outputPosition] = 0;
            aligned[c] = dry[c][dryPosition]; dry[c][dryPosition] = in[c];
        }
        position = (position + 1) % Transform::size;
        outputPosition = (outputPosition + 1) % maximumHop;
        dryPosition = (dryPosition + 1) % latency;
        if (++counter == interval)
        {
            processFrame(); counter = 0; interval = target.highQuality ? 512 : maximumHop;
        }
        smooth(mix, std::clamp(target.mix, 0.0f, 1.0f)); smooth(gain, targetGain);
        smooth(delta, target.delta ? 1.0f : 0.0f); smooth(bypass, target.bypass ? 1.0f : 0.0f);
        for (int c = 0; c < 2; ++c)
        {
            const float normal = aligned[c] + mix * (wet[c] - aligned[c]);
            const float monitored = normal + delta * ((aligned[c] - wet[c]) - normal);
            result[c] = (1 - bypass) * monitored * gain + bypass * aligned[c];
        }
        return result;
    }
    const SoftDetector& diagnostics(int channel) const noexcept
    { return chains[(target.midSide ? 2 : 0) + channel].detector; }
private:
    struct Chain
    {
        SoftDetector detector;
        HardDetector hard;
        DetailProcessor detail;
        BandWeighting weight;
        Curve wanted{}, amplitude{};
    };
    void processFrame() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            for (std::size_t i = 0; i < FFT::size; ++i)
                work[c][i] = input[c][(position + Transform::size - FFT::size + i) % Transform::size] * analysisWindow[i];
            analysisFFT.transform(work[c], false);
        }
        for (std::size_t i = 0; i < Transform::size; ++i)
            scratch[i] = {input[0][(position + i) % Transform::size], input[1][(position + i) % Transform::size]};
        fft.transform(scratch, false);
        for (std::size_t k = 0; k < Transform::size; ++k)
        {
            const auto a = scratch[k], b = std::conj(scratch[(Transform::size - k) % Transform::size]);
            raw[0][k] = (a + b) * 0.5f; raw[1][k] = (a - b) * std::complex<float>(0, -0.5f);
        }
        for (std::size_t k = 0; k < FFT::size; ++k)
        {
            work[2][k] = (work[0][k] + work[1][k]) * 0.5f;
            work[3][k] = (work[0][k] - work[1][k]) * 0.5f;
        }
        if (!focusValid || !(cachedBands == target.bands))
        {
            cachedBands = target.bands; focusValid = true;
            for (int c = 0; c < 2; ++c) for (std::size_t k = 0; k < SoftDetector::bins; ++k)
                focus[c][k] = bandFocusGain(static_cast<float>(k * rate / FFT::size), target.bands, c);
        }
        for (int c = 0; c < 4; ++c)
        {
            auto& chain = chains[c]; chain.detector.setTiming(target.attack, target.release, interval);
            if (target.hard) { chain.detector.analyzeMagnitude(work[c]); chain.hard.analyze(chain.detector.magnitude); }
            else chain.detector.analyze(work[c]);
            chain.detail.process(target.hard ? chain.hard.raw : chain.detector.raw, target.detail);
            chain.weight.apply(chain.detail.result, target.bands);
        }
        for (int base : {0, 2})
        {
            for (std::size_t k = 0; k < SoftDetector::bins; ++k)
            {
                const float a = chains[base].weight.result[k], b = chains[base + 1].weight.result[k];
                const float shared = std::max(a, b), link = std::clamp(target.link, 0.0f, 1.0f);
                chains[base].wanted[k] = (a + link * (shared - a)) * focus[0][k];
                chains[base + 1].wanted[k] = (b + link * (shared - b)) * focus[1][k];
            }
            for (int c = base; c < base + 2; ++c)
            {
                chains[c].detector.attenuate(work[c], chains[c].wanted, target.depth);
                for (std::size_t k = 0; k < SoftDetector::bins; ++k)
                    chains[c].amplitude[k] = std::pow(10.0f, -chains[c].detector.reduction[k] / 20);
            }
        }
        const float targetMode = target.midSide ? 1.0f : 0.0f;
        modeBlend = targetMode + std::exp(-static_cast<float>(interval / rate) / 0.03f) * (modeBlend - targetMode);
        for (std::size_t k = 0; k < SoftDetector::bins; ++k)
        {
            const float same = 0.5f * (chains[2].amplitude[k] + chains[3].amplitude[k]);
            matrix[0][k] = (1 - modeBlend) * chains[0].amplitude[k] + modeBlend * same;
            matrix[1][k] = (1 - modeBlend) * chains[1].amplitude[k] + modeBlend * same;
            matrix[2][k] = modeBlend * 0.5f * (chains[2].amplitude[k] - chains[3].amplitude[k]);
        }
        designer.designPair(matrix[0], matrix[1], fft, next[0], next[1]);
        if (modeBlend == 0) next[2].fill({});
        else designer.design(matrix[2], fft, next[2]);
        auto convolve = [&](const auto& filters)
        {
            for (std::size_t k = 0; k < Transform::size; ++k)
            {
                const auto left = raw[0][k] * filters[0][k] + raw[1][k] * filters[2][k];
                const auto right = raw[1][k] * filters[1][k] + raw[0][k] * filters[2][k];
                scratch[k] = left + std::complex<float>(-right.imag(), right.real());
            }
            fft.transform(scratch, true);
        };
        convolve(previous);
        for (std::size_t i = 0; i < interval; ++i)
        {
            oldOutput[0][i] = scratch[Transform::size - interval + i].real();
            oldOutput[1][i] = scratch[Transform::size - interval + i].imag();
        }
        convolve(next);
        for (std::size_t i = 0; i < interval; ++i)
        {
            const float blend = static_cast<float>(i + 1) / interval;
            const std::array<float, 2> value{{scratch[Transform::size - interval + i].real(), scratch[Transform::size - interval + i].imag()}};
            for (int c = 0; c < 2; ++c)
            {
                // Constant 1024-sample block delay even when Quality changes hop.
                output[c][(outputPosition + maximumHop - interval + i) % maximumHop] = oldOutput[c][i] + blend * (value[c] - oldOutput[c][i]);
            }
        }
        previous = next;
    }
    void smooth(float& current, float wanted) const noexcept
    {
        current = wanted + coefficient * (current - wanted);
        if (std::abs(current - wanted) < 1.0e-6f) current = wanted;
    }
    FFT analysisFFT;
    Transform fft;
    LinearPhaseDesigner designer;
    std::array<Chain, 4> chains;
    std::array<FFT::Spectrum, 4> work{};
    std::array<Spectrum, 2> raw{};
    std::array<Spectrum, 3> previous{}, next{};
    Spectrum scratch{};
    std::array<Curve, 3> matrix{};
    std::array<Curve, 2> focus{};
    std::array<std::array<float, Transform::size>, 2> input{};
    std::array<std::array<float, maximumHop>, 2> output{};
    std::array<std::array<float, latency>, 2> dry{};
    std::array<float, FFT::size> analysisWindow{};
    std::array<std::array<float, maximumHop>, 2> oldOutput{};
    Bands cachedBands{};
    Settings target;
    double rate = 48000;
    std::size_t position = 0, outputPosition = 0, dryPosition = 0, counter = 0, interval = maximumHop;
    bool monoInput = false, focusValid = false;
    float coefficient = 0, modeBlend = 0, mix = 1, gain = 1, targetGain = 1, delta = 0, bypass = 0;
};
}
