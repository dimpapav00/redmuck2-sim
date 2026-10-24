#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout RedMuckAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto knob = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    auto db = [&p] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (lo, hi, 0.1f), def));
    };
    db   ("input",      "Input",       -24.0f, 24.0f, 0.0f);
    knob ("gain",       "Gain",        0.60f);
    knob ("tone",       "Tone",        0.50f);
    knob ("level",      "Level",       0.50f);
    knob ("boostlevel", "Boost Level", 0.50f);
    db   ("output",     "Output",      -24.0f, 12.0f, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "Effect On", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "boost", 1 }, "Boost On", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "clip", 1 }, "Clipping",
                                                               juce::StringArray { "Symmetrical", "Asymmetrical +" }, 0));
    return { p.begin(), p.end() };
}

RedMuckAudioProcessor::RedMuckAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pIn = apvts.getRawParameterValue ("input");
    pGain = apvts.getRawParameterValue ("gain");
    pTone = apvts.getRawParameterValue ("tone");
    pLevel = apvts.getRawParameterValue ("level");
    pBoostLvl = apvts.getRawParameterValue ("boostlevel");
    pOut = apvts.getRawParameterValue ("output");
    pOn = apvts.getRawParameterValue ("on");
    pBoost = apvts.getRawParameterValue ("boost");
    pClip = apvts.getRawParameterValue ("clip");
}

void RedMuckAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool RedMuckAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void RedMuckAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    redmuck::Params p;
    p.inputDb    = pIn->load();
    p.gain       = pGain->load();
    p.tone       = pTone->load();
    p.level      = pLevel->load();
    p.boostLevel = pBoostLvl->load();
    p.outputDb   = pOut->load();
    p.on         = pOn->load() > 0.5f;
    p.boost      = pBoost->load() > 0.5f;
    p.clip       = (int) std::lround (pClip->load());

    engine.process (buffer, p);
}

juce::AudioProcessorEditor* RedMuckAudioProcessor::createEditor() { return new RedMuckEditor (*this); }

void RedMuckAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RedMuckAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RedMuckAudioProcessor(); }
