#include "Engine.h"
#include <iostream>
#include <memory>
#include <random>
#include <vector>

int main()
{
    auto engine = std::make_unique<soothe::Engine>();
    soothe::Settings settings;
    double lastReduction = -1;
    // Test monotonic Depth in the final pipeline, with Detail enabled.
    for (float depth : {0.0f, 2.5f, 5.0f, 7.5f, 10.0f})
    {
        settings.depth = depth; engine->prepare(48000, settings);
        double energy = 0;
        for (int i = 0; i < 96000; ++i)
        {
            const float output = engine->tick(static_cast<float>(0.3 * std::sin(6.283185307179586 * i / 48)));
            if (i >= 48000) energy += output * output;
        }
        const double reduction = 10 * std::log10(2160 / energy);
        if (reduction < lastReduction - 0.001) return 1;
        lastReduction = reduction;
        std::cout << "final Depth=" << depth << " reduction=" << reduction << '\n';
    }
    settings = {}; settings.delta = true; engine->prepare(48000, settings);
    double sine = 0, cosine = 0, energy = 0;
    for (int i = 0; i < 96000; ++i)
    {
        const auto phase = 6.283185307179586 * i / 48;
        const float output = engine->tick(static_cast<float>(0.3 * std::sin(phase)));
        if (i >= 48000) { sine += output * std::sin(phase); cosine += output * std::cos(phase); energy += output * output; }
    }
    const double toneFraction = 2 * (sine * sine + cosine * cosine) / (48000 * energy);
    std::cout << "Delta 1kHz energy fraction=" << toneFraction << '\n';
    if (toneFraction < 0.999) return 2;

    // Relative-gain behavior on a broadband-plus-resonance signal, beyond pure sine.
    std::mt19937 generator(42);
    std::uniform_real_distribution<float> noise(-0.08f, 0.08f);
    std::vector<float> input(96000), reference(96000);
    for (int i = 0; i < 96000; ++i) input[i] = noise(generator) + static_cast<float>(0.2 * std::sin(6.283185307179586 * i / 48));
    settings = {}; engine->prepare(48000, settings);
    for (std::size_t i = 0; i < input.size(); ++i) reference[i] = engine->tick(input[i]);
    engine->prepare(48000, settings);
    float maximumError = 0;
    for (std::size_t i = 0; i < input.size(); ++i)
        maximumError = std::max(maximumError, std::abs(4 * engine->tick(input[i] * 0.25f) - reference[i]));
    std::cout << "Broadband scale null=" << maximumError << '\n';
    if (maximumError > 1.0e-5f) return 3;

    // Parameter transitions under a constant input should not create an impulsive step.
    settings = {}; settings.depth = 0; engine->prepare(48000, settings);
    float previous = 0, maximumStep = 0;
    for (int i = 0; i < 48000; ++i)
    {
        if (i == 16000) { settings.outputDb = 6; engine->setSettings(settings); }
        if (i == 24000) { settings.bypass = true; engine->setSettings(settings); }
        if (i == 32000) { settings.bypass = false; settings.delta = true; engine->setSettings(settings); }
        const float current = engine->tick(0.2f);
        if (i > 10000) maximumStep = std::max(maximumStep, std::abs(current - previous));
        previous = current;
    }
    std::cout << "Gain/bypass/Delta transition max step=" << maximumStep << '\n';
    return maximumStep < 0.003f ? 0 : 4;
}
