/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "juce_core/juce_core.h"
#include "kLookAndFeel.h"

//==============================================================================
/**
*/
class QwikRefAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    QwikRefAudioProcessorEditor (QwikRefAudioProcessor&);
    ~QwikRefAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    QwikRefAudioProcessor& audioProcessor;

    Laf lnf;
    juce::Array<juce::Font> fonts;

    juce::URL url{ "https://kwhaley5.gumroad.com/" };

    juce::HyperlinkButton gumroad{ "Plugins", url };

    juce::ToggleButton car, laptop, phone, tv, airpods, speaker, power;
    juce::AudioProcessorValueTreeState::ButtonAttachment carAT, laptopAT, phoneAT, tvAT, airpodsAT, speakerAT, powerAT;

    juce::ApplicationProperties appProperties;

    int orgWidth{200}, orgHeight{230};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QwikRefAudioProcessorEditor)
};
