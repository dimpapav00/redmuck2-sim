#pragma once
#include "PluginProcessor.h"

class MuckLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MuckLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class RedMuckEditor : public juce::AudioProcessorEditor
{
public:
    explicit RedMuckEditor (RedMuckAudioProcessor&);
    ~RedMuckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void addKnob (Knob&, const juce::String& id, const juce::String& text);

    RedMuckAudioProcessor& proc;
    MuckLookAndFeel lnf;

    Knob gain, tone, level, boostLevel;
    juce::ToggleButton boostSwitch { "BOOST" }, onSwitch { "ON / OFF" };
    juce::ComboBox clipBox;
    juce::Label clipLabel, inLabel, outLabel;
    juce::Slider inSlider, outSlider;

    std::unique_ptr<ButtonAtt> boostAtt, onAtt;
    std::unique_ptr<ComboAtt> clipAtt;
    std::unique_ptr<SliderAtt> inAtt, outAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedMuckEditor)
};
