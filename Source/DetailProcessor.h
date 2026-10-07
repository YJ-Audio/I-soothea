#pragma once
#include "SoftDetector.h"

namespace soothe
{
// Peak-preserving Gaussian envelope on a uniform log-frequency grid.
// max_j(raw[j] * Gaussian(log(f/f_j))) avoids diluting narrow high-frequency
// peaks merely because FFT bin spacing is linear. This is a tunable hypothesis.
class DetailProcessor
{
public:
    using Curve = SoftDetector::Curve;
    static constexpr std::size_t gridSize = 2049;
    static constexpr float octaveSpan = 11.0f;
    static constexpr float widestSigmaOctaves = 0.6f;
    void prepare() noexcept
    {
        for (std::size_t k = 0; k < SoftDetector::bins; ++k)
            binGrid[k] = std::log2(static_cast<float>(std::max(std::size_t(1), k))) * (gridSize - 1) / octaveSpan;
    }
    void process(const Curve& raw, float detail) noexcept
    {
        const float t = std::clamp(detail / 10, 0.0f, 1.0f);
        const float strength = 0.4f + 1.6f * t;
        const float sigma = widestSigmaOctaves * std::pow(0.2f, 2 * t) * (1 - std::pow(t, 8));
        const double sigmaBins = sigma * (gridSize - 1) / octaveSpan;
        if (sigmaBins < 0.5)
        {
            for (std::size_t k = 0; k < raw.size(); ++k) result[k] = raw[k] * strength;
            return;
        }
        // Max-splat onto the log grid so that even a single high FFT bin is retained.
        a.fill(0);
        for (std::size_t k = 1; k < raw.size(); ++k)
        {
            const auto location = static_cast<std::size_t>(std::round(binGrid[k]));
            a[location] = std::max(a[location], raw[k]);
        }
        for (std::size_t i = 0; i < gridSize; ++i)
            costs[i] = -std::log(std::max(1.0e-20, static_cast<double>(a[i])));

        // A squared-distance lower envelope computes this max-product smoothing
        // in O(gridSize), independent of width. Parabola intersections use doubles.
        const double coefficient = 0.5 / (sigmaBins * sigmaBins);
        int envelope = 0;
        vertices[0] = 0; intersections[0] = -1.0e30; intersections[1] = 1.0e30;
        for (int q = 1; q < static_cast<int>(gridSize); ++q)
        {
            double crossing;
            do
            {
                const int v = vertices[envelope];
                crossing = ((costs[q] + coefficient * q * q) - (costs[v] + coefficient * v * v)) / (2 * coefficient * (q - v));
                if (crossing > intersections[envelope]) break;
                --envelope;
            } while (envelope >= 0);
            ++envelope;
            vertices[envelope] = q; intersections[envelope] = crossing; intersections[envelope + 1] = 1.0e30;
        }
        envelope = 0;
        for (int q = 0; q < static_cast<int>(gridSize); ++q)
        {
            while (intersections[envelope + 1] < q) ++envelope;
            const double distance = q - vertices[envelope];
            b[q] = static_cast<float>(std::exp(-coefficient * distance * distance - costs[vertices[envelope]]));
        }
        for (std::size_t k = 0; k < raw.size(); ++k)
            result[k] = std::max(raw[k], interpolate(b, binGrid[k])) * strength;
    }
    Curve result{};
private:
    template <std::size_t N>
    static float interpolate(const std::array<float, N>& values, float index) noexcept
    {
        index = std::clamp(index, 0.0f, static_cast<float>(N - 1));
        const auto lower = static_cast<std::size_t>(index);
        const auto upper = std::min(lower + 1, N - 1);
        return values[lower] + (index - static_cast<float>(lower)) * (values[upper] - values[lower]);
    }
    std::array<float, gridSize> a{}, b{};
    std::array<double, gridSize> costs{};
    std::array<double, gridSize + 1> intersections{};
    std::array<int, gridSize> vertices{};
    Curve binGrid{};
};

}
