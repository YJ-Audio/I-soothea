#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorEditor* SootheCloneProcessor::createEditor() { return new SootheCloneEditor(*this); }
bool SootheCloneProcessor::readDisplay(DisplayFrame& frame) noexcept
{
    int expected = 2;
    if (!displayState.compare_exchange_strong(expected, 3, std::memory_order_acquire)) return false;
    frame = display;
    displayState.store(0, std::memory_order_release);
    return true;
}
void SootheCloneProcessor::publishDisplay(int channels) noexcept
{
    int expected = 0;
    if (!displayState.compare_exchange_strong(expected, 1, std::memory_order_acquire)) return;
    display.upperFrequency = static_cast<float>(std::min(20000.0, rate * 0.5));
    for (int p = 0; p < DisplayFrame::points; ++p)
    {
        float magnitude = 0, reduction = 0;
        auto accumulate = [&](const auto& data)
        {
            for (int k = displayBins[p]; k <= displayBins[p + 1]; ++k)
            {
                magnitude = std::max(magnitude, data.magnitude[static_cast<std::size_t>(k)]);
                reduction = std::max(reduction, data.reduction[static_cast<std::size_t>(k)]);
            }
        };
        for (int c = 0; c < channels; ++c)
            if (isLinearPhaseActive()) accumulate(linearEngine.diagnostics(c));
            else if (isLowLatencyActive())
            {
                if (channels == 1) accumulate(lowMonoEngine.diagnostics()); else accumulate(lowStereoEngine.diagnostics(c));
            }
            else
            {
                if (channels == 1) accumulate(monoEngine.diagnostics()); else accumulate(stereoEngine.diagnostics(c));
            }
        const float normalization = 1024.0f;
        display.inputDb[p] = 20 * std::log10(std::max(1.0e-6f, magnitude / normalization));
        display.reductionDb[p] = reduction;
    }
    displayState.store(2, std::memory_order_release);
}

juce::AudioProcessorValueTreeState::ParameterLayout SootheCloneProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&](const char* id, const char* name, float low, float high, float initial)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>(low, high), initial));
    };
    add("depth", "Depth", 0, 10, 5);
    add("detail", "Detail", 0, 10, 5);
    add("attack", "Attack (ms)", 1, 200, 20);
    add("release", "Release (ms)", 5, 1000, 120);
    add("mix", "Dry / Wet", 0, 1, 1);
    add("output", "Output (dB)", -24, 12, 0);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"delta", 1}, "Delta", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    const auto defaults = soothe::defaultBands();
    for (int i = 0; i < 5; ++i)
    {
        const auto& b = defaults[static_cast<std::size_t>(i)];
        const auto name = "Band " + juce::String(i + 1) + " ";
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{bandID(i, "enabled"), 1}, name + "Enabled", b.enabled));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{bandID(i, "shape"), 1}, name + "Shape",
            juce::StringArray{"Bell", "Low shelf", "High shelf", "Low cut", "High cut"}, static_cast<int>(b.shape)));
        auto frequencyRange = juce::NormalisableRange<float>(20, 20000); frequencyRange.setSkewForCentre(1000);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{bandID(i, "freq"), 1}, name + "Frequency", frequencyRange, b.frequency));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{bandID(i, "amount"), 1}, name + "Weight (dB)", juce::NormalisableRange<float>(-24, 24), b.amountDb));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{bandID(i, "width"), 1}, name + "Width (oct)", juce::NormalisableRange<float>(0.15f, 4), b.widthOctaves));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{bandID(i, "slope"), 1}, name + "Slope (dB/oct)", juce::NormalisableRange<float>(6, 48), b.slope));
    }
    // Append to preserve existing parameter indices as well as IDs.
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Mode", juce::StringArray{"Soft", "Hard"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"stereoMode", 1}, "Stereo mode", juce::StringArray{"L/R", "M/S"}, 0));
    add("link", "Stereo link", 0, 1, 0);
    for (int i = 0; i < 5; ++i)
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{bandID(i, "focus"), 1}, "Band " + juce::String(i + 1) + " Focus", juce::NormalisableRange<float>(-1, 1), 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality", 1}, "Quality", juce::StringArray{"Normal", "High"}, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"lowLatency", 1}, "Low latency", false, juce::AudioParameterBoolAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"linearPhase", 1}, "Linear phase", false, juce::AudioParameterBoolAttributes().withAutomatable(false)));
    return layout;
}

SootheCloneProcessor::SootheCloneProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "SootheCloneState", makeParameters())
{
    const char* ids[] = {"depth", "detail", "attack", "release", "mix", "output", "delta", "bypass"};
    for (std::size_t i = 0; i < 8; ++i) values[i] = parameters.getRawParameterValue(ids[i]);
    linearPhaseValue = parameters.getRawParameterValue("linearPhase");
    lowLatencyValue = parameters.getRawParameterValue("lowLatency");
    qualityValue = parameters.getRawParameterValue("quality");
    modeValue = parameters.getRawParameterValue("mode");
    stereoValue = parameters.getRawParameterValue("stereoMode"); linkValue = parameters.getRawParameterValue("link");
    const char* fields[] = {"enabled", "shape", "freq", "amount", "width", "slope", "focus"};
    for (int i = 0; i < 5; ++i)
        for (int field = 0; field < 7; ++field) bandValues[i][field] = parameters.getRawParameterValue(bandID(i, fields[field]));
    setLatencySamples(static_cast<int>(soothe::STFT::latency));
}
soothe::Settings SootheCloneProcessor::settings() const noexcept
{
    soothe::Settings s;
    s.depth = values[0]->load(); s.detail = values[1]->load();
    s.attack = values[2]->load(); s.release = values[3]->load();
    s.mix = values[4]->load(); s.outputDb = values[5]->load();
    s.delta = values[6]->load() >= 0.5f; s.bypass = values[7]->load() >= 0.5f;
    s.bands = getBands();
    s.hard = isHardMode(); s.midSide = isMidSide(); s.link = linkValue->load(); s.highQuality = qualityValue->load() >= 0.5f;
    return s;
}
soothe::Bands SootheCloneProcessor::getBands() const noexcept
{
    auto bands = soothe::defaultBands();
    for (int i = 0; i < 5; ++i)
    {
        auto& b = bands[static_cast<std::size_t>(i)];
        const auto& v = bandValues[static_cast<std::size_t>(i)];
        b.enabled = v[0]->load() >= 0.5f; b.shape = static_cast<soothe::BandShape>(static_cast<int>(v[1]->load()));
        b.frequency = v[2]->load(); b.amountDb = v[3]->load(); b.widthOctaves = v[4]->load(); b.slope = v[5]->load(); b.focus = v[6]->load();
    }
    return bands;
}
void SootheCloneProcessor::prepareToPlay(double sampleRate, int)
{
    rate = sampleRate > 0 ? sampleRate : 48000;
    displayRate.store(rate);
    activeLinearPhase.store(isLinearPhaseRequested());
    activeLowLatency.store(!isLinearPhaseActive() && lowLatencyValue->load() >= 0.5f);
    const int fftSize = 4096;
    for (int p = 0; p <= DisplayFrame::points; ++p)
    {
        const double frequency = 20 * std::pow(std::min(20000.0, rate * 0.5) / 20, static_cast<double>(p) / DisplayFrame::points);
        displayBins[p] = std::clamp(static_cast<int>(frequency * fftSize / rate), 0, fftSize / 2);
    }
    displayCountdown = 0;
    reset();
    setLatencySamples(isLinearPhaseActive() ? 3072 : isLowLatencyActive() ? 2048 : 4096);
}
void SootheCloneProcessor::reset()
{
    const auto s = settings();
    if (isLinearPhaseActive()) linearEngine.prepare(rate, s, getTotalNumInputChannels() == 1);
    else if (isLowLatencyActive()) { lowMonoEngine.prepare(rate, s); lowStereoEngine.prepare(rate, s); }
    else { monoEngine.prepare(rate, s); stereoEngine.prepare(rate, s); }
}
bool SootheCloneProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet();
    return input == layout.getMainOutputChannelSet()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}
void SootheCloneProcessor::process(juce::AudioBuffer<float>& buffer, bool forcedBypass)
{
    juce::ScopedNoDenormals noDenormals;
    auto s = settings(); s.bypass = s.bypass || forcedBypass;
    const int channels = std::min(2, buffer.getNumChannels());
    auto processMono = [&](auto& engine)
    {
        engine.setSettings(s);
        auto* samples = buffer.getWritePointer(0);
        for (int i = 0; i < buffer.getNumSamples(); ++i) samples[i] = engine.tick(samples[i]);
    };
    auto processStereo = [&](auto& engine)
    {
        engine.setSettings(s);
        auto* left = buffer.getWritePointer(0); auto* right = buffer.getWritePointer(1);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto output = engine.tick(left[i], right[i]); left[i] = output[0]; right[i] = output[1];
        }
    };
    if (isLinearPhaseActive() && channels > 0)
    {
        linearEngine.setSettings(s);
        auto* left = buffer.getWritePointer(0);
        auto* right = channels == 2 ? buffer.getWritePointer(1) : nullptr;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto out = linearEngine.tick(left[i], right != nullptr ? right[i] : 0);
            left[i] = out[0]; if (right != nullptr) right[i] = out[1];
        }
    }
    else
    if (channels == 1)
    {
        if (isLowLatencyActive()) processMono(lowMonoEngine); else processMono(monoEngine);
    }
    else if (channels == 2)
    {
        if (isLowLatencyActive()) processStereo(lowStereoEngine); else processStereo(stereoEngine);
    }
    for (int channel = channels; channel < buffer.getNumChannels(); ++channel) buffer.clear(channel, 0, buffer.getNumSamples());
    displayCountdown -= buffer.getNumSamples();
    if (displayCountdown <= 0 && buffer.getNumSamples() > 0)
    {
        publishDisplay(channels);
        displayCountdown = std::max(1, static_cast<int>(rate / 30));
    }
}
void SootheCloneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, false); }
void SootheCloneProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }
juce::AudioProcessorParameter* SootheCloneProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
void SootheCloneProcessor::getStateInformation(juce::MemoryBlock& data)
{
    if (const auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, data);
}
void SootheCloneProcessor::setStateInformation(const void* data, int size)
{
    if (const auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            if (!restored.getChildWithProperty("id", "mode").isValid())
            {
                juce::ValueTree mode("PARAM"); mode.setProperty("id", "mode", nullptr); mode.setProperty("value", 0, nullptr);
                restored.addChild(mode, -1, nullptr);
            }
            for (const auto* id : {"stereoMode", "link", "quality", "lowLatency", "linearPhase"})
                if (!restored.getChildWithProperty("id", id).isValid())
                {
                    juce::ValueTree child("PARAM"); child.setProperty("id", id, nullptr); child.setProperty("value", 0, nullptr);
                    restored.addChild(child, -1, nullptr);
                }
            // Older sessions have no bands. Restore neutral defaults, not the
            // current band's potentially edited values from a different session.
            const char* fields[] = {"enabled", "shape", "freq", "amount", "width", "slope", "focus"};
            for (int i = 0; i < 5; ++i)
                for (const auto* field : fields)
                {
                    const auto id = bandID(i, field);
                    if (!restored.getChildWithProperty("id", id).isValid())
                    {
                        auto* p = parameters.getParameter(id);
                        juce::ValueTree child("PARAM"); child.setProperty("id", id, nullptr);
                        child.setProperty("value", p->convertFrom0to1(p->getDefaultValue()), nullptr);
                        restored.addChild(child, -1, nullptr);
                    }
                }
            parameters.replaceState(restored);
        }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SootheCloneProcessor(); }
