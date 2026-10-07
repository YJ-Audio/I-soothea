#include "Engine.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <vector>
#include <cstdint>
#include <xmmintrin.h>
#include <pmmintrin.h>

namespace fs = std::filesystem;
constexpr int rate = 48000, samples = rate * 2, latency = 4096, tail = 8192;
struct Case { std::string name; std::vector<double> frequencies; int level = -12; float detail = 5; bool noise = false; };
void u32(std::ostream& out, std::uint32_t x)
{ for (int i = 0; i < 4; ++i) out.put(static_cast<char>((x >> (8*i)) & 255)); }
void u16(std::ostream& out, std::uint16_t x)
{ out.put(static_cast<char>(x & 255)); out.put(static_cast<char>(x >> 8)); }
void wav(const fs::path& path, const std::vector<float>& audio)
{
    std::ofstream out(path, std::ios::binary); out.exceptions(std::ios::badbit | std::ios::failbit);
    const auto bytes = static_cast<std::uint32_t>(audio.size() * sizeof(float));
    out.write("RIFF", 4); u32(out, 48 + bytes); out.write("WAVEfmt ", 8); u32(out, 16);
    u16(out, 3); u16(out, 1); u32(out, rate); u32(out, rate * 4); u16(out, 4); u16(out, 32);
    out.write("fact", 4); u32(out, 4); u32(out, static_cast<std::uint32_t>(audio.size()));
    out.write("data", 4); u32(out, bytes);
    out.write(reinterpret_cast<const char*>(audio.data()), bytes);
}
int main(int argc, char** argv)
try
{
    if (argc < 2 || argc > 3 || (argc == 3 && std::string(argv[2]) != "--smoke"))
    { std::cerr << "Usage: SootheMeasurements OUTPUT_DIRECTORY [--smoke]\n"; return 1; }
    const fs::path directory = fs::u8path(argv[1]);
    if (fs::exists(directory) && !fs::is_empty(directory))
    { std::cerr << "Output directory must be empty; existing exports are not overwritten.\n"; return 2; }
    fs::create_directories(directory);
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    std::vector<Case> cases;
    for (int frequency : {100, 200, 500, 1000, 2000, 5000, 10000}) for (int level : {-6, -12, -24, -36})
        cases.push_back({"sine_" + std::to_string(frequency) + "Hz_" + std::to_string(-level) + "dB", {double(frequency)}, level});
    for (int detail : {0, 5, 10}) cases.push_back({"three_tones_detail" + std::to_string(detail), {500, 1000, 2000}, -12, float(detail)});
    cases.push_back({"white_noise", {}, -12, 5, true}); cases.push_back({"silence", {}});
    if (argc == 3) cases = {{"sine_1000Hz_12dB", {1000}, -12}, {"silence", {}}};
    std::ofstream summary(directory / "measurements.csv"); summary.exceptions(std::ios::badbit | std::ios::failbit);
    summary << "case,mode,detail,input_peak_dbfs,input_rms,wet_rms,reduction_db,delta_null_max\n" << std::setprecision(9);
    auto wetEngine = std::make_unique<soothe::Engine>(), deltaEngine = std::make_unique<soothe::Engine>();
    double worstNull = 0; int renders = 0;
    for (const auto& test : cases)
    {
        std::vector<float> input(samples), dry(samples + tail), wet(dry.size()), removed(dry.size());
        std::mt19937 random(1979);
        float peak = 0;
        for (int i = 0; i < samples; ++i)
        {
            double value = test.noise ? 2.0 * double(random()) / double(std::mt19937::max()) - 1.0 : 0;
            for (auto frequency : test.frequencies) value += std::sin(6.283185307179586 * frequency * i / rate);
            // Five-ms end ramps avoid hard edges in the listening files.
            const float ramp = std::min({1.0f, i / 240.0f, (samples - 1 - i) / 240.0f});
            input[i] = static_cast<float>(value) * ramp; peak = std::max(peak, std::abs(input[i]));
        }
        for (int i = 0; i < samples; ++i)
        {
            if (peak > 0) input[i] *= std::pow(10.0f, test.level / 20.0f) / peak;
            dry[i + latency] = input[i];
        }
        wav(directory / (test.name + "_input_aligned.wav"), dry);
        for (bool hard : {false, true})
        {
            soothe::Settings settings; settings.hard = hard; settings.detail = test.detail;
            wetEngine->prepare(rate, settings); settings.delta = true; deltaEngine->prepare(rate, settings);
            const std::string name = test.name + (hard ? "_hard" : "_soft");
            std::ofstream curve(directory / (name + "_curve.csv")); curve.exceptions(std::ios::badbit | std::ios::failbit);
            curve << "frequency_hz,fft_magnitude,soft_baseline_db,soft_resonance_db,soft_raw_db,reduction_db\n" << std::setprecision(9);
            double nullError = 0, inputEnergy = 0, wetEnergy = 0;
            for (int i = 0; i < static_cast<int>(dry.size()); ++i)
            {
                const float in = i < samples ? input[i] : 0;
                wet[i] = wetEngine->tick(in); removed[i] = deltaEngine->tick(in);
                if (!std::isfinite(wet[i]) || !std::isfinite(removed[i])) throw std::runtime_error("Non-finite output");
                nullError = std::max(nullError, std::abs(double(dry[i]) - wet[i] - removed[i]));
                if (i >= latency + rate && i < latency + 91200)
                { inputEnergy += double(dry[i])*dry[i]; wetEnergy += double(wet[i])*wet[i]; }
                if (i == samples - 1)
                {
                    const auto& d = wetEngine->diagnostics();
                    for (std::size_t k = 0; k < d.bins; ++k)
                    {
                        curve << double(k)*rate/4096 << ',' << d.magnitude[k] << ',';
                        if (!hard) curve << d.baseline[k] << ',' << d.resonance[k] << ',' << d.raw[k];
                        else curve << ",,"; // Soft-only diagnostic arrays are stale in Hard.
                        curve << ',' << d.reduction[k] << '\n';
                    }
                }
            }
            if (nullError > 2e-6) throw std::runtime_error("Delta reconstruction failed");
            wav(directory / (name + "_wet.wav"), wet); wav(directory / (name + "_delta.wav"), removed);
            summary << test.name << ',' << (hard ? "Hard" : "Soft") << ',' << test.detail << ',';
            if (peak > 0) summary << test.level;
            summary << ',' << std::sqrt(inputEnergy/43200) << ',' << std::sqrt(wetEnergy/43200) << ',';
            if (inputEnergy > 0 && wetEnergy > 0) summary << 10*std::log10(inputEnergy/wetEnergy);
            summary << ',' << nullError << '\n';
            worstNull = std::max(worstNull, nullError); ++renders;
        }
    }
    std::cout << "renders=" << renders << " deltaNullMax=" << worstNull << '\n';
}
catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 3; }
