#include "PluginEditor.h"

namespace
{
const juce::Colour ink{0xff43536f}, blue{0xff778fbd}, pale{0xffccdbeb}, white{0xfff2f7fc};
const juce::Rectangle<float> graph{246, 87, 832, 454};
const std::array<juce::Colour, 5> bandColours{{juce::Colour(0xffe37b9d), juce::Colour(0xffd6a356), juce::Colour(0xff4aaac9), juce::Colour(0xff9d82d0), juce::Colour(0xff50a797)}};
float weightY(float db) { return graph.getCentreY() - db * graph.getHeight() / 48; }
void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<float> bounds, float size,
          juce::Justification alignment = juce::Justification::centred)
{
    g.setFont(juce::FontOptions(size));
    g.drawText(value, bounds, alignment);
}
}
SootheLook::SootheLook()
{
    setColour(juce::Slider::textBoxTextColourId, ink);
    setColour(juce::Slider::textBoxBackgroundColourId, pale);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::backgroundColourId, pale);
    setColour(juce::Slider::trackColourId, blue);
    setColour(juce::Slider::thumbColourId, white);
    setColour(juce::TextButton::buttonColourId, pale);
    setColour(juce::TextButton::textColourOffId, ink);
}
void SootheLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                float position, float start, float end, juce::Slider&)
{
    auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(7);
    const float radius = std::min(area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const float thickness = radius * 0.14f;
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius - thickness / 2, radius - thickness / 2, 0, start, end, true);
    g.setColour(blue); g.strokePath(arc, juce::PathStrokeType(thickness));
    arc.clear(); arc.addCentredArc(centre.x, centre.y, radius - thickness / 2, radius - thickness / 2, 0, start, start + position * (end - start), true);
    g.setColour(white); g.strokePath(arc, juce::PathStrokeType(thickness));
    auto disc = juce::Rectangle<float>(radius * 1.65f, radius * 1.65f).withCentre(centre);
    g.setColour(ink.withAlpha(0.15f)); g.fillEllipse(disc.translated(1, 4).expanded(2));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff535765), disc.getTopLeft(), juce::Colour(0xff303644), disc.getBottomRight(), false));
    g.fillEllipse(disc);
}
void SootheLook::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool)
{
    auto r = button.getLocalBounds().toFloat().reduced(1);
    if (button.getComponentID() == "modeChoice")
    {
        g.setColour(button.getToggleState() ? ink : pale); g.fillRoundedRectangle(r, r.getHeight() / 2);
        g.setColour(button.getToggleState() ? white : ink); text(g, button.getButtonText(), r, std::min(16.0f, r.getHeight() * 0.65f));
        return;
    }
    g.setColour(button.getToggleState() ? white : pale.withAlpha(over ? 1.0f : 0.8f));
    g.fillRoundedRectangle(r, r.getHeight() / 2);
    auto dot = juce::Rectangle<float>(14, 14).withCentre({r.getX() + 17, r.getCentreY()});
    g.setColour(ink); g.drawEllipse(dot, 1.5f);
    if (button.getToggleState()) g.fillEllipse(dot.reduced(3));
    text(g, button.getButtonText(), r.withTrimmedLeft(31).withTrimmedRight(5), std::min(15.0f, r.getHeight() * 0.52f), juce::Justification::centredLeft);
}
juce::Label* SootheLook::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V2::createSliderTextBox(slider);
    label->setColour(juce::Label::textColourId, ink);
    label->setColour(juce::Label::backgroundColourId, pale);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setFont(juce::FontOptions(17.0f));
    return label;
}
SootheCloneEditor::SootheCloneEditor(SootheCloneProcessor& processor) : AudioProcessorEditor(processor), owner(processor)
{
    setLookAndFeel(&look);
    const char* ids[] = {"depth", "detail", "attack", "release", "mix", "output"};
    const char* labels[] = {"Depth", "Detail", "Attack", "Release", "Mix", "Output"};
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        auto& slider = knobs[i];
        slider.setLookAndFeel(&look);
        slider.setName(labels[i]); slider.setTitle(labels[i]);
        slider.setSliderStyle(i < 4 ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, i < 4 ? 74 : 92, 26);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setNumDecimalPlacesToDisplay(i == 2 || i == 3 ? 0 : 1);
        addAndMakeVisible(slider);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters, ids[i], slider);
        slider.textFromValueFunction = [i](double value)
        {
            if (i == 4) return juce::String(value * 100, 0) + " %";
            return juce::String(value, i == 2 || i == 3 ? 0 : 1) + (i == 2 || i == 3 ? " ms" : i == 5 ? " dB" : "");
        };
        slider.valueFromTextFunction = [i](const juce::String& value) { return value.getDoubleValue() / (i == 4 ? 100 : 1); };
        slider.updateText();
    }
    knobs[4].textFromValueFunction = [](double v) { return juce::String(v * 100, 0) + " %"; };
    knobs[4].valueFromTextFunction = [](const juce::String& s) { return s.getDoubleValue() / 100; };
    knobs[0].setTooltip("Resonance reduction strength. Double-click to reset.");
    knobs[1].setTooltip("Low: broad reduction. High: narrow, independent resonances.");
    bypass.setTooltip("Monitor latency-aligned dry audio."); delta.setTooltip("Listen to the removed signal: dry minus wet.");
    for (auto* button : {&bypass, &delta, &spectrum}) addAndMakeVisible(button);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters, "bypass", bypass);
    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters, "delta", delta);
    spectrum.setToggleState(true, juce::dontSendNotification); spectrum.onClick = [this] { repaint(); };
    softMode.setName("Soft mode"); hardMode.setName("Hard mode");
    for (auto* mode : {&softMode, &hardMode})
    {
        mode->setComponentID("modeChoice"); mode->setRadioGroupId(101); addAndMakeVisible(mode);
        mode->onClick = [this, mode]
        {
            if (!mode->getToggleState()) return;
            auto* p = owner.parameters.getParameter("mode"); p->beginChangeGesture();
            p->setValueNotifyingHost(mode == &hardMode ? 1.0f : 0.0f); p->endChangeGesture();
            updateModeButtons(); repaint();
        };
    }
    softMode.setTooltip("Relative spectral prominence: less dependent on input level.");
    hardMode.setTooltip("Absolute-level detection with frequency-dependent sensitivity. Lower input levels receive less reduction.");
    updateModeButtons();
    addAndMakeVisible(resetButton);
    resetButton.setTooltip("Reset all processing parameters to their defaults.");
    resetButton.onClick = [this]
    {
        for (auto* p : owner.getParameters()) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
    };
    linearPhase.setName("Linear phase"); addAndMakeVisible(linearPhase);
    linearPhaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters, "linearPhase", linearPhase);
    linearPhase.setTooltip("Symmetric FIR filtering. 3072 samples latency. Save and reload to apply. Overrides Low Latency; uses more CPU. Dynamic reduction can still cause modulation.");
    lowLatency.setName("Low latency"); addAndMakeVisible(lowLatency);
    lowLatencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters, "lowLatency", lowLatency);
    lowLatency.setTooltip("Halves latency using a shorter synthesis window; keeps analysis resolution. Save and reload the plugin, or restart the audio engine, to apply. No live latency changes.");
    quality.setName("Quality"); quality.addItemList({"Normal", "High"}, 1);
    quality.setColour(juce::ComboBox::backgroundColourId, pale); quality.setColour(juce::ComboBox::textColourId, ink);
    addAndMakeVisible(quality);
    qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters, "quality", quality);
    quality.setTooltip("High doubles analysis updates and increases CPU use. Sensitivity and latency stay unchanged.");
    stereoMode.setName("Stereo mode"); stereoMode.addItemList({"L/R", "M/S"}, 1);
    stereoMode.setColour(juce::ComboBox::backgroundColourId, pale); stereoMode.setColour(juce::ComboBox::textColourId, ink);
    addAndMakeVisible(stereoMode);
    stereoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters, "stereoMode", stereoMode);
    stereoLink.setLookAndFeel(&look);
    stereoLink.setName("Stereo link"); stereoLink.setSliderStyle(juce::Slider::LinearBar);
    stereoLink.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 28); addAndMakeVisible(stereoLink);
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters, "link", stereoLink);
    stereoLink.textFromValueFunction = [](double v) { return "Link " + juce::String(v * 100, 0) + "%"; };
    stereoLink.valueFromTextFunction = [](const juce::String& v) { return v.retainCharacters("0123456789.-").getDoubleValue() / 100; };
    stereoLink.updateText();
    stereoLink.setTooltip("0%: independent. 100%: share the stronger reduction. Band Focus is applied afterwards.");
    stereoMode.setTooltip("L/R: left and right. M/S: mid (L+R)/2 and side (L-R)/2. Changes crossfade smoothly.");
    frame.inputDb.fill(-120);
    bandSelect.setName("Band selection"); bandShape.setName("Band shape");
    for (int i = 0; i < 5; ++i) bandSelect.addItem("Band " + juce::String(i + 1), i + 1);
    bandShape.addItemList({"Bell", "Low shelf", "High shelf", "Low cut", "High cut"}, 1);
    for (auto* combo : {&bandSelect, &bandShape})
    {
        combo->setColour(juce::ComboBox::backgroundColourId, pale);
        combo->setColour(juce::ComboBox::textColourId, ink);
        combo->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(combo);
    }
    addAndMakeVisible(bandEnabled);
    for (std::size_t i = 0; i < bandControls.size(); ++i)
    {
        auto& control = bandControls[i]; control.setLookAndFeel(&look);
        control.setName(juce::StringArray{"Band frequency", "Band strength", "Band width", "Band slope", "Band focus"}[static_cast<int>(i)]);
        control.setSliderStyle(juce::Slider::LinearBar); control.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 110, 28);
        addAndMakeVisible(control);
    }
    bandSelect.onChange = [this] { selectBand(bandSelect.getSelectedId() - 1); };
    bandShape.onChange = [this] { updateBandControls(); repaint(); };
    selectBand(selectedBand);
    bandEnabled.setTooltip("Enable or disable only the selected band's processing weight.");
    bandControls[1].setTooltip("Scales reduction, not audio gain. +6 dB is approximately double the reduction; -6 dB is approximately half.");
    bandControls[2].setTooltip("Bell half-height width in octaves; shelf transition width.");
    bandControls[4].setTooltip("Selected-band processing balance: negative = Left/Mid, positive = Right/Side. Centre treats both equally. Mono ignores Focus.");
    bandControls[3].setTooltip("How sharply processing is reduced outside the cutoff, in dB per octave.");
    setResizable(true, true); setResizeLimits(880, 544, 1650, 1020);
    getConstrainer()->setFixedAspectRatio(1100.0 / 680);
    setSize(1100, 680); startTimerHz(30);
}
SootheCloneEditor::~SootheCloneEditor() { stopTimer(); finishDrag(); stereoLink.setLookAndFeel(nullptr); for (auto& knob : knobs) knob.setLookAndFeel(nullptr); for (auto& knob : bandControls) knob.setLookAndFeel(nullptr); setLookAndFeel(nullptr); }
void SootheCloneEditor::updateModeButtons()
{
    const bool hard = owner.isHardMode(); softMode.setToggleState(!hard, juce::dontSendNotification); hardMode.setToggleState(hard, juce::dontSendNotification);
}
void SootheCloneEditor::timerCallback() { linearPhase.setButtonText(owner.isLinearPhaseRequested() != owner.isLinearPhaseActive() ? "reload to apply" : "linear phase"); lowLatency.setEnabled(!owner.isLinearPhaseRequested()); lowLatency.setButtonText(!owner.isLinearPhaseRequested() && owner.isLatencyChangePending() ? "reload to apply" : "low latency"); if (owner.readDisplay(frame)) receivedFrame = true; updateBandControls(); updateModeButtons(); repaint(); }
void SootheCloneEditor::resized()
{
    const float sx = getWidth() / 1100.0f, sy = getHeight() / 680.0f;
    auto place = [&](juce::Component& c, int x, int y, int w, int h)
    { c.setBounds(juce::Rectangle<float>(static_cast<float>(x) * sx, static_cast<float>(y) * sy, static_cast<float>(w) * sx, static_cast<float>(h) * sy).toNearestInt()); };
    place(knobs[0], 24, 183, 166, 192); place(knobs[1], 57, 412, 100, 124);
    place(knobs[2], 15, 572, 88, 99); place(knobs[3], 110, 572, 88, 99);
    place(bypass, 226, 639, 108, 30); place(delta, 344, 639, 91, 30);
    place(knobs[4], 487, 640, 105, 28); place(knobs[5], 643, 640, 112, 28);
    place(spectrum, 821, 10, 121, 29); place(resetButton, 978, 10, 98, 29);
    place(linearPhase, 943, 639, 137, 30);
    place(lowLatency, 432, 10, 166, 29);
    place(quality, 838, 640, 99, 28);
    place(stereoMode, 609, 10, 83, 29); place(stereoLink, 703, 10, 108, 29);
    place(bandControls[4], 1000, 587, 78, 29);
    place(softMode, 48, 140, 58, 28); place(hardMode, 106, 140, 58, 28);
    place(bandSelect, 246, 587, 87, 29); place(bandEnabled, 341, 587, 97, 29);
    place(bandShape, 450, 587, 125, 29); place(bandControls[0], 592, 587, 121, 29);
    place(bandControls[1], 728, 587, 120, 29);
    place(bandControls[2], 863, 587, 126, 29); place(bandControls[3], 863, 587, 126, 29);
}
void SootheCloneEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(getWidth() / 1100.0f, getHeight() / 680.0f));
    g.fillAll(juce::Colour(0xffa4bdd7));
    g.setColour(juce::Colour(0xff9fb7d2)); g.fillRect(0, 0, 212, 680);
    g.setColour(ink.withAlpha(0.15f)); g.fillRect(210, 0, 2, 680);
    g.setColour(juce::Colour(0xff5667a1)); text(g, "I Soothea", {24, 25, 174, 52}, 34, juce::Justification::centredLeft);

    g.setColour(ink.withAlpha(0.75f)); text(g, "YJ Audio", {32, 105, 148, 20}, 12, juce::Justification::centredLeft);
#if JUCE_DEBUG
    g.setColour(juce::Colour(0xff9b293c));
    text(g, "DEBUG / HIGH CPU", {32, 123, 166, 15}, 11, juce::Justification::centredLeft);
#else
    text(g, "Release build", {32, 123, 166, 15}, 11, juce::Justification::centredLeft);
#endif
    g.setColour(ink);
    for (auto item : {std::pair<const char*, juce::Rectangle<float>>{"depth", {0, 170, 212, 23}}, {"detail", {0, 383, 212, 25}}, {"attack", {11, 545, 94, 23}}, {"release", {107, 545, 94, 23}}})
        text(g, item.first, item.second, 17);
    g.setColour(pale.withAlpha(0.65f)); g.fillRect(212, 0, 888, 49);
    g.setColour(ink); text(g, juce::String(owner.isHardMode() ? "Hard" : "Soft") + " / " + juce::String(owner.activeLatencyMs(), 1) + " ms", {238, 9, 187, 30}, 18, juce::Justification::centredLeft);
    const auto plot = graph;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb0c7de), 0, 87, juce::Colour(0xffbed0e3), 0, 607, false));
    g.fillRect(plot);
    const float zero = plot.getCentreY();
    g.setColour(blue.withAlpha(0.58f)); g.fillRect(plot.withTop(zero));
    const auto xFor = [&](float hz) { return plot.getX() + plot.getWidth() * std::log(hz / 20) / std::log(frame.upperFrequency / 20); };
    g.setColour(ink.withAlpha(0.82f));
    for (float hz : {20.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f})
    {
        if (hz > frame.upperFrequency) continue;
        const float x = xFor(hz);
        g.setColour(white.withAlpha(0.14f)); g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
        g.setColour(ink.withAlpha(0.85f)); text(g, hz >= 1000 ? juce::String(hz / 1000, 0) + "k" : juce::String(hz, 0), {x - 22, 57, 44, 22}, 13);
    }
    for (int db = 0; db <= 24; db += 6)
    {
        const float y = zero + (plot.getBottom() - zero) * db / 24;
        g.setColour(white.withAlpha(db == 0 ? 0.5f : 0.16f)); g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        g.setColour(ink.withAlpha(0.8f)); text(g, db == 0 ? "0" : "-" + juce::String(db), {1040, y - 20, 33, 18}, 12, juce::Justification::centredRight);
    }
    g.setColour(white.withAlpha(0.16f)); g.drawHorizontalLine(static_cast<int>(weightY(12)), plot.getX(), plot.getRight());
    g.setColour(ink.withAlpha(0.8f)); text(g, "+12", {1040, weightY(12) - 20, 33, 18}, 12, juce::Justification::centredRight);
    if (spectrum.getToggleState())
    {
        juce::Path input;
        input.startNewSubPath(plot.getBottomLeft());
        for (int p = 0; p < SootheCloneProcessor::DisplayFrame::points; ++p)
        {
            const float x = plot.getX() + plot.getWidth() * p / (SootheCloneProcessor::DisplayFrame::points - 1);
            const float y = plot.getBottom() - juce::jlimit(0.0f, 1.0f, (frame.inputDb[p] + 100) / 100) * plot.getHeight();
            input.lineTo(x, y);
        }
        input.lineTo(plot.getBottomRight()); input.closeSubPath();
        g.setColour(ink.withAlpha(0.16f)); g.fillPath(input);
    }
    juce::Path reduction;
    float maximum = 0;
    for (int p = 0; p < SootheCloneProcessor::DisplayFrame::points; ++p)
    {
        const float x = plot.getX() + plot.getWidth() * p / (SootheCloneProcessor::DisplayFrame::points - 1);
        const float y = zero + (plot.getBottom() - zero) * std::min(24.0f, frame.reductionDb[p]) / 24;
        if (p == 0) reduction.startNewSubPath(x, y); else reduction.lineTo(x, y);
        maximum = std::max(maximum, frame.reductionDb[p]);
    }
    g.setColour(juce::Colour(0xff477da8)); g.strokePath(reduction, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved));
    juce::Path weighting;
    const auto bands = owner.getBands();
    for (int p = 0; p <= 512; ++p)
    {
        const float frequency = 20 * std::pow(frame.upperFrequency / 20, p / 512.0f);
        const float x = plot.getX() + plot.getWidth() * p / 512;
        const float y = std::clamp(weightY(soothe::bandWeightDb(frequency, bands)), plot.getY(), plot.getBottom());
        if (p == 0) weighting.startNewSubPath(x, y); else weighting.lineTo(x, y);
    }
    g.setColour(white); g.strokePath(weighting, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved));
    for (int i = 0; i < 5; ++i)
    {
        if (!bands[i].enabled) continue;
        const auto point = nodePosition(i);
        const auto bounds = juce::Rectangle<float>(16, 16).withCentre(point);
        if (i == selectedBand) { g.setColour(white.withAlpha(0.5f)); g.drawEllipse(bounds.expanded(4), 1.5f); }
        g.setColour(bandColours[i].withAlpha(bands[i].enabled ? 1.0f : 0.4f)); g.fillEllipse(bounds);
        g.setColour(white); g.drawEllipse(bounds, 2);
        g.setColour(ink); text(g, juce::String(i + 1), {point.x - 10, point.y + 12, 20, 18}, 10);
    }
    g.setColour(ink); text(g, "REDUCTION", {263, 102, 110, 22}, 11, juce::Justification::centredLeft);
    text(g, juce::String(maximum, 1) + " dB", {263, 126, 150, 34}, 28, juce::Justification::centredLeft);
    text(g, "WHITE: WEIGHT   BLUE: REDUCTION", {790, 102, 267, 20}, 10, juce::Justification::centredRight);
    if (!receivedFrame) { g.setColour(ink.withAlpha(0.6f)); text(g, "Play audio to view the spectrum", {570, 206, 380, 25}, 15); }
    g.setColour(ink);
    text(g, "FREQUENCY", {592, 562, 120, 20}, 10, juce::Justification::centredLeft);
    text(g, "STRENGTH", {728, 562, 120, 20}, 10, juce::Justification::centredLeft);
    const bool cut = static_cast<int>(bands[selectedBand].shape) >= 3;
    text(g, cut ? "SLOPE" : "WIDTH", {863, 562, 120, 20}, 10, juce::Justification::centredLeft);
    text(g, owner.isMidSide() ? "FOCUS M / S" : "FOCUS L / R", {995, 562, 88, 20}, 10);
    const bool full = std::all_of(bands.begin(), bands.end(), [](const auto& b) { return b.enabled; });
    text(g, full ? "5/5 POINTS: DOUBLE-CLICK A POINT TO REMOVE" : "DOUBLE-CLICK: ADD / REMOVE (MAX 5 POINTS)", {246, 545, 340, 25}, 10, juce::Justification::centredLeft);
    g.setColour(pale.withAlpha(0.38f)); g.fillRect(212, 628, 888, 52);
    g.setColour(ink); text(g, "mix", {443, 639, 40, 30}, 16); text(g, "out", {601, 639, 40, 30}, 16);
    text(g, "quality", {766, 639, 69, 30}, 14);
}

void SootheCloneEditor::selectBand(int index)
{
    finishDrag(); selectedBand = std::clamp(index, 0, 4);
    bandSelect.setSelectedId(selectedBand + 1, juce::dontSendNotification);
    const char* fields[] = {"freq", "amount", "width", "slope", "focus"};
    for (std::size_t i = 0; i < bandControls.size(); ++i)
    {
        bandAttachments[i].reset();
        auto& slider = bandControls[i];
        bandAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.parameters, owner.bandID(selectedBand, fields[i]), slider);
        slider.textFromValueFunction = [i](double v)
        { if (i == 4) return juce::String(v * 100, 0) + "%"; return juce::String(v, i == 0 || i == 3 ? 0 : i == 2 ? 2 : 1) + (i == 0 ? " Hz" : i == 1 ? " dB" : i == 2 ? " oct" : " dB/oct"); };
        slider.valueFromTextFunction = [i](const juce::String& textValue) { return textValue.getDoubleValue() / (i == 4 ? 100 : 1); };
        slider.updateText();
    }
    bandEnabledAttachment.reset(); bandShapeAttachment.reset();
    bandEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters, owner.bandID(selectedBand, "enabled"), bandEnabled);
    bandShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters, owner.bandID(selectedBand, "shape"), bandShape);
    updateBandControls(); repaint();
}
void SootheCloneEditor::updateBandControls()
{
    const bool stereo = owner.getTotalNumInputChannels() == 2;
    stereoMode.setEnabled(stereo); stereoLink.setEnabled(stereo); bandControls[4].setEnabled(stereo);
    const bool cut = static_cast<int>(owner.getBands()[selectedBand].shape) >= 3;
    bandControls[1].setEnabled(!cut); bandControls[2].setVisible(!cut); bandControls[3].setVisible(cut);
}
juce::Point<float> SootheCloneEditor::nodePosition(int index) const
{
    const auto bands = owner.getBands(); const auto& b = bands[index];
    const float x = graph.getX() + graph.getWidth() * std::log(std::clamp(b.frequency, 20.0f, frame.upperFrequency) / 20) / std::log(frame.upperFrequency / 20);
    const float y = static_cast<int>(b.shape) >= 3 ? weightY(0) : weightY(b.amountDb);
    return {x, y};
}
juce::Point<float> SootheCloneEditor::designPoint(const juce::MouseEvent& e) const { return {e.position.x * 1100 / getWidth(), e.position.y * 680 / getHeight()}; }
int SootheCloneEditor::hitBand(juce::Point<float> point) const
{
    float nearest = 20; int hit = -1;
    const auto bands = owner.getBands();
    for (int i = 0; i < 5; ++i) { if (!bands[i].enabled) continue; const float distance = point.getDistanceFrom(nodePosition(i)); if (distance < nearest) { nearest = distance; hit = i; } }
    return hit;
}
void SootheCloneEditor::setBandValue(int index, const char* field, float value, bool gesture)
{
    auto* parameter = owner.parameters.getParameter(owner.bandID(index, field));
    if (gesture) parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    if (gesture) parameter->endChangeGesture();
    repaint();
}
void SootheCloneEditor::mouseDown(const juce::MouseEvent& e)
{
    const int hit = hitBand(designPoint(e)); if (hit < 0) return;
    selectBand(hit);
    if (e.mods.isPopupMenu()) { setBandValue(hit, "enabled", owner.getBands()[hit].enabled ? 0.0f : 1.0f); return; }
    draggedBand = hit; setBandValue(hit, "enabled", 1);
    for (const auto* field : {"freq", "amount"}) owner.parameters.getParameter(owner.bandID(hit, field))->beginChangeGesture();
}
void SootheCloneEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedBand < 0) return;
    const auto point = designPoint(e);
    const float t = std::clamp((point.x - graph.getX()) / graph.getWidth(), 0.0f, 1.0f);
    setBandValue(draggedBand, "freq", 20 * std::pow(frame.upperFrequency / 20, t), false);
    if (static_cast<int>(owner.getBands()[draggedBand].shape) < 3)
        setBandValue(draggedBand, "amount", std::clamp((weightY(0) - point.y) * 48 / graph.getHeight(), -24.0f, 24.0f), false);
}
void SootheCloneEditor::finishDrag()
{
    if (draggedBand >= 0) for (const auto* field : {"freq", "amount"}) owner.parameters.getParameter(owner.bandID(draggedBand, field))->endChangeGesture();
    draggedBand = -1;
}
void SootheCloneEditor::mouseUp(const juce::MouseEvent&) { finishDrag(); }
void SootheCloneEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    finishDrag();
    const auto point = designPoint(e);
    const int hit = hitBand(point);
    if (hit >= 0)
    {
        selectBand(hit); setBandValue(hit, "enabled", 0);
        return;
    }
    if (!graph.contains(point)) return;
    const auto bands = owner.getBands();
    for (int i = 0; i < static_cast<int>(bands.size()); ++i)
    {
        if (bands[i].enabled) continue;
        // Reuse a stable parameter slot; initialize while disabled, enable last.
        const float t = (point.x - graph.getX()) / graph.getWidth();
        setBandValue(i, "shape", static_cast<float>(soothe::BandShape::bell));
        setBandValue(i, "freq", 20 * std::pow(frame.upperFrequency / 20, t));
        setBandValue(i, "amount", std::clamp((weightY(0) - point.y) * 48 / graph.getHeight(), -24.0f, 24.0f));
        setBandValue(i, "width", 1); setBandValue(i, "slope", 24); setBandValue(i, "focus", 0);
        setBandValue(i, "enabled", 1); selectBand(i);
        return;
    }
}
void SootheCloneEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int hit = hitBand(designPoint(e)); if (hit < 0) return;
    selectBand(hit); const auto b = owner.getBands()[hit]; const bool cut = static_cast<int>(b.shape) >= 3;
    setBandValue(hit, cut ? "slope" : "width", cut ? std::clamp(b.slope + wheel.deltaY * 12, 6.0f, 48.0f)
        : std::clamp(b.widthOctaves * std::pow(2.0f, wheel.deltaY), 0.15f, 4.0f));
}
