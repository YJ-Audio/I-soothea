#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <cstddef>

namespace soothe
{
// Fixed-size radix-2 FFT: tables are built before audio starts. No locks or heap use.
template <std::size_t N>
class BasicFFT
{
public:
    static constexpr std::size_t size = N;
    using Spectrum = std::array<std::complex<float>, size>;
    BasicFFT()
    {
        for (std::size_t i = 0; i < size; ++i)
        {
            std::size_t x = i, reversed = 0;
            for (std::size_t bits = size; bits > 1; bits >>= 1) { reversed = (reversed << 1) | (x & 1); x >>= 1; }
            permutation[i] = reversed;
        }
        for (std::size_t i = 0; i < size / 2; ++i)
            roots[i] = std::polar(1.0f, -6.283185307179586f * static_cast<float>(i) / size);
    }
    void transform(Spectrum& data, bool inverse) const noexcept
    {
        for (std::size_t i = 0; i < size; ++i)
            if (i < permutation[i]) std::swap(data[i], data[permutation[i]]);
        for (std::size_t length = 2; length <= size; length *= 2)
            for (std::size_t base = 0; base < size; base += length)
                for (std::size_t j = 0; j < length / 2; ++j)
                {
                    auto root = roots[j * size / length];
                    if (inverse) root = std::conj(root);
                    const auto a = data[base + j];
                    const auto b = data[base + j + length / 2] * root;
                    data[base + j] = a + b;
                    data[base + j + length / 2] = a - b;
                }
        if (inverse) for (auto& value : data) value /= static_cast<float>(size);
    }
private:
    std::array<std::size_t, size> permutation{};
    std::array<std::complex<float>, size / 2> roots{};
};

using FFT = BasicFFT<4096>;

template <std::size_t N, std::size_t L = N>
class BasicSTFT
{
public:
    using FFT = BasicFFT<N>;
    using Spectrum = typename FFT::Spectrum;
    static constexpr std::size_t size = FFT::size, hop = size / 4, latency = L;
    static_assert(L == N || L == N / 2, "Supported synthesis lengths are N and N/2");
    BasicSTFT()
    {
        for (std::size_t i = 0; i < size; ++i)
            window[i] = static_cast<float>(0.5 - 0.5 * std::cos(6.283185307179586 * i / size));
        for (std::size_t i = 0; i < latency; ++i)
            synthesisWindow[i] = static_cast<float>(0.5 - 0.5 * std::cos(6.283185307179586 * i / latency));
        reset();
    }
    void reset(std::size_t interval = hop) noexcept
    {
        input.fill(0); output.fill(0); normalization.fill(0); position = 0; hopCounter = 0;
        requestedHop = currentHop = frameHop = interval == hop / 2 ? hop / 2 : hop;
        // Include zero-input frames from before reset, avoiding startup gain lift.
        const float scale = static_cast<float>(currentHop) / (size * 0.375f);
        for (std::size_t i = 0; i < size; ++i)
            for (std::size_t j = i; j < latency; j += currentHop)
                normalization[i] += window[size - latency + j] * synthesisWindow[j] * scale;
    }
    void setHighQuality(bool high) noexcept { requestedHop = high ? hop / 2 : hop; }
    std::size_t frameInterval() const noexcept { return frameHop; }
    template <typename SpectralProcessor>
    float tick(float sample, SpectralProcessor&& processSpectrum) noexcept
    {
        const float result = push(sample);
        if (frameReady()) { processSpectrum(spectrum); synthesise(); }
        return result;
    }
    // Paired channels can analyse both frames before either inverse FFT runs.
    float push(float sample) noexcept
    {
        // Normalize the actual overlap, including mixed hop sizes while switching.
        const float result = normalization[position] > 1.0e-8f ? output[position] / normalization[position] : 0;
        output[position] = 0;
        normalization[position] = 0;
        input[position] = sample;
        position = (position + 1) % size;
        if (++hopCounter == currentHop)
        {
            hopCounter = 0;
            frameHop = currentHop; currentHop = requestedHop;
            for (std::size_t i = 0; i < size; ++i)
                spectrum[i] = {input[(position + i) % size] * window[i], 0};
            fft.transform(spectrum, false);
        }
        return result;
    }
    bool frameReady() const noexcept { return hopCounter == 0; }
    Spectrum& frame() noexcept { return spectrum; }
    void synthesise() noexcept
    {
        fft.transform(spectrum, true);
        const float scale = static_cast<float>(frameHop) / (size * 0.375f);
        for (std::size_t i = 0; i < latency; ++i)
        {
            const auto index = (position + i) % size;
            output[index] += spectrum[size - latency + i].real() * synthesisWindow[i] * scale;
            normalization[index] += window[size - latency + i] * synthesisWindow[i] * scale;
        }
    }
private:
    FFT fft;
    Spectrum spectrum{};
    std::array<float, size> window{}, synthesisWindow{}, input{}, output{}, normalization{};
    std::size_t position = 0, hopCounter = 0;
    std::size_t requestedHop = hop, currentHop = hop, frameHop = hop;
};
using STFT = BasicSTFT<4096>;
}
