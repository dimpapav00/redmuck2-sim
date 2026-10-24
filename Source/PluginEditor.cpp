#include "PluginEditor.h"

namespace
{
const juce::Colour kRed    { 0xffa3231c };
const juce::Colour kRed2   { 0xff5c120f };
const juce::Colour kCream  { 0xfff0e3c8 };
const juce::Colour kLed    { 0xffff4a3a };
}

MuckLookAndFeel::MuckLookAndFeel()
{
    setColour (juce::Label::textColourId, kCream);
    setColour (juce::ComboBox::backgroundColourId, kRed2);
    setColour (juce::ComboBox::textColourId, kCream);
    setColour (juce::ComboBox::outlineColourId, kCream.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId, kCream);
    setColour (juce::PopupMenu::backgroundColourId, kRed2);
    setColour (juce::PopupMenu::textColourId, kCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kCream.withAlpha (0.25f));
    setColour (juce::Slider::trackColourId, kCream);
    setColour (juce::Slider::thumbColourId, kCream);
    setColour (juce::Slider::backgroundColourId, kRed2);
    setColour (juce::TextEditor::textColourId, kCream);
    setColour (juce::TextEditor::backgroundColourId, kRed2);
}

void MuckLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (kCream.withAlpha (0.8f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float r1 = radius, r2 = radius - (i % 5 == 0 ? 7.0f : 4.0f);
        g.drawLine (centre.x + std::sin (a) * r1, centre.y - std::cos (a) * r1,
                    centre.x + std::sin (a) * r2, centre.y - std::cos (a) * r2, 1.2f);
    }

    const float kr = radius - 12.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b3b), centre.x - kr, centre.y - kr,
                                             juce::Colour (0xff070707), centre.x + kr, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff5a5a5a));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-2.0f, -kr + 2.0f, 4.0f, kr * 0.5f, 1.5f);
    g.setColour (kCream);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void MuckLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto area = b.getLocalBounds().toFloat();
    const float d = std::min (area.getWidth() * 0.6f, area.getHeight() - 30.0f);
    const auto sw = juce::Rectangle<float> (d, d).withCentre ({ area.getCentreX(), area.getY() + 22.0f + d * 0.5f });

    const bool on = b.getToggleState();
    const juce::Rectangle<float> led (area.getCentreX() - 6.0f, area.getY() + 2.0f, 12.0f, 12.0f);
    if (on)
    {
        g.setColour (kLed.withAlpha (0.4f));
        g.fillEllipse (led.expanded (5.0f));
    }
    g.setColour (on ? kLed : juce::Colour (0xff4a1b16));
    g.fillEllipse (led);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd2d2d2), sw.getX(), sw.getY(),
                                             juce::Colour (0xff6e6e6e), sw.getRight(), sw.getBottom(), false));
    g.fillEllipse (sw);
    g.setColour (highlighted ? kCream : juce::Colour (0xff1d1d1d));
    g.drawEllipse (sw, 2.0f);

    g.setColour (kCream);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

RedMuckEditor::RedMuckEditor (RedMuckAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (600, 480);

    addKnob (gain,       "gain",       "GAIN");
    addKnob (tone,       "tone",       "TONE");
    addKnob (level,      "level",      "LEVEL");
    addKnob (boostLevel, "boostlevel", "BOOST LEVEL");

    addAndMakeVisible (boostSwitch);
    addAndMakeVisible (onSwitch);
    boostAtt = std::make_unique<ButtonAtt> (proc.apvts, "boost", boostSwitch);
    onAtt    = std::make_unique<ButtonAtt> (proc.apvts, "on", onSwitch);

    clipBox.addItemList ({ "Symmetrical", "Asymmetrical +" }, 1);
    addAndMakeVisible (clipBox);
    clipAtt = std::make_unique<ComboAtt> (proc.apvts, "clip", clipBox);

    for (auto* l : { &clipLabel, &inLabel, &outLabel })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    }
    clipLabel.setText ("CLIPPING", juce::dontSendNotification);
    inLabel.setText ("INPUT", juce::dontSendNotification);
    outLabel.setText ("OUTPUT", juce::dontSendNotification);

    for (auto* s : { &inSlider, &outSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        s->setTextValueSuffix (" dB");
        addAndMakeVisible (s);
    }
    inAtt  = std::make_unique<SliderAtt> (proc.apvts, "input", inSlider);
    outAtt = std::make_unique<SliderAtt> (proc.apvts, "output", outSlider);
}

RedMuckEditor::~RedMuckEditor() { setLookAndFeel (nullptr); }

void RedMuckEditor::addKnob (Knob& k, const juce::String& id, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void RedMuckEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff140605));
    const auto body = getLocalBounds().toFloat().reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (kRed.brighter (0.1f), 0, 0, kRed2, 0, (float) getHeight(), false));
    g.fillRoundedRectangle (body, 12.0f);
    g.setColour (juce::Colour (0xff2a0a08));
    g.drawRoundedRectangle (body, 12.0f, 3.0f);

    g.setColour (kCream);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("REDMUCK2 SIM", getLocalBounds().removeFromTop (60).withTrimmedTop (14), juce::Justification::centred);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (kCream.withAlpha (0.85f));
    g.drawText ("fuzz / distortion with boost", getLocalBounds().removeFromTop (82).withTrimmedTop (60),
                juce::Justification::centred);
    g.setColour (kCream.withAlpha (0.55f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("unofficial emulation, not affiliated with any manufacturer",
                getLocalBounds().removeFromBottom (22), juce::Justification::centred);
}

void RedMuckEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    r.removeFromTop (78);

    auto row = r.removeFromTop (140);
    const int w = row.getWidth() / 4;
    for (auto* k : { &gain, &tone, &level, &boostLevel })
    {
        auto cell = row.removeFromLeft (w);
        k->label.setBounds (cell.removeFromBottom (22));
        k->slider.setBounds (cell);
    }

    r.removeFromTop (6);
    auto mid = r.removeFromTop (110);
    boostSwitch.setBounds (mid.removeFromLeft (mid.getWidth() / 2).withSizeKeepingCentre (130, 110));
    onSwitch.setBounds (mid.withSizeKeepingCentre (130, 110));

    r.removeFromTop (6);
    auto clipRow = r.removeFromTop (30).withSizeKeepingCentre (280, 30);
    clipLabel.setBounds (clipRow.removeFromLeft (80));
    clipBox.setBounds (clipRow);

    r.removeFromTop (8);
    auto bottom = r.removeFromTop (30);
    auto half = bottom.removeFromLeft (bottom.getWidth() / 2);
    inLabel.setBounds (half.removeFromLeft (52));
    inSlider.setBounds (half.reduced (4, 0));
    outLabel.setBounds (bottom.removeFromLeft (60));
    outSlider.setBounds (bottom);
}
