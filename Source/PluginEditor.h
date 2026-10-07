#pragma once
#include "PluginProcessor.h"

class SootheLook final : public juce::LookAndFeel_V4
{
public:
    SootheLook();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override { return juce::FontOptions(std::min(15.0f, box.getHeight() * 0.45f)); }
};

class SootheCloneEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SootheCloneEditor(SootheCloneProcessor&);
    ~SootheCloneEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override;
    SootheCloneProcessor& owner;
    SootheLook look;
    std::array<juce::Slider, 6> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> attachments;
    juce::ToggleButton bypass{"bypass"}, delta{"delta"}, spectrum{"spectrum"};
    juce::ToggleButton lowLatency{"low latency"}, linearPhase{"linear phase"};
    juce::ToggleButton softMode{"soft"}, hardMode{"hard"};
    void updateModeButtons();
    juce::TextButton resetButton{"Reset"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, deltaAttachment, lowLatencyAttachment, linearPhaseAttachment;
    SootheCloneProcessor::DisplayFrame frame;
    juce::TooltipWindow tooltips{this, 700};
    bool receivedFrame = false;
    juce::ComboBox stereoMode, quality;
    juce::Slider stereoLink;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoAttachment, qualityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> linkAttachment;
    juce::ComboBox bandSelect, bandShape;
    juce::ToggleButton bandEnabled{"enabled"};
    std::array<juce::Slider, 5> bandControls;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> bandAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandEnabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandShapeAttachment;
    int selectedBand = 2, draggedBand = -1;
    void selectBand(int);
    void updateBandControls();
    void finishDrag();
    juce::Point<float> nodePosition(int) const;
    juce::Point<float> designPoint(const juce::MouseEvent&) const;
    int hitBand(juce::Point<float>) const;
    void setBandValue(int, const char*, float, bool gesture = true);
};
