/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
QwikRefAudioProcessor::QwikRefAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    car = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("car"));
    laptop = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("laptop"));
    phone = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("phone"));
    tv = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("tv"));
    airpods = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("airpods"));
    btSpeaker = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("btSpeaker"));
    power = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("power"));

}

QwikRefAudioProcessor::~QwikRefAudioProcessor()
{
}

//==============================================================================
const juce::String QwikRefAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool QwikRefAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool QwikRefAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool QwikRefAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double QwikRefAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int QwikRefAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int QwikRefAudioProcessor::getCurrentProgram()
{
    return 0;
}

void QwikRefAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String QwikRefAudioProcessor::getProgramName (int index)
{
    return {};
}

void QwikRefAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void QwikRefAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    for(int i = 0; i < 2; i++)
    {
        lowCut[i].prepare(spec);
        peak1[i].prepare(spec);
        peak2[i].prepare(spec);
        peak3[i].prepare(spec);
        highCut[i].prepare(spec);
    }

}

void QwikRefAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool QwikRefAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void QwikRefAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (power->get()) { return; }

    auto sampleRate = getSampleRate();

    juce::dsp::AudioBlock<float> blockRight;

    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto blockLeft = block.getSingleChannelBlock(0);
    blockRight = totalNumOutputChannels == 1 ? block.getSingleChannelBlock(0) : block.getSingleChannelBlock(1);
    auto contextLeft = juce::dsp::ProcessContextReplacing<float>(blockLeft);
    auto contextRight = juce::dsp::ProcessContextReplacing<float>(blockRight);

    if (car->get())
    {
        float db5 = juce::Decibels::decibelsToGain(5.f);
        float dbn5 = juce::Decibels::decibelsToGain(-5.f);

        auto lowCutCoef = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 100, .71, db5);
        lowCut[0].coefficients = lowCutCoef;
        lowCut[1].coefficients = lowCutCoef;
        lowCut[0].process(contextLeft);
        lowCut[1].process(contextRight);

        auto peak1Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 625, .71, dbn5);
        peak1[0].coefficients = peak1Coef;
        peak1[1].coefficients = peak1Coef;
        peak1[0].process(contextLeft);
        peak1[1].process(contextRight);

        auto peak2Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 1600, .71, dbn5);
        peak2[0].coefficients = peak2Coef;
        peak2[1].coefficients = peak2Coef;
        peak2[0].process(contextLeft);
        peak2[1].process(contextRight);

        auto peak3Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 4000, .71, db5);
        peak3[0].coefficients = peak3Coef;
        peak3[1].coefficients = peak3Coef;
        peak3[0].process(contextLeft);
        peak3[1].process(contextRight);

        auto highCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 10000, .71, juce::Decibels::decibelsToGain(10.f));
        highCut[0].coefficients = highCutCoef;
        highCut[1].coefficients = highCutCoef;
        highCut[0].process(contextLeft);
        highCut[1].process(contextRight);
    }
    
    else if (laptop->get())
    {
        float db10 = juce::Decibels::decibelsToGain(10.f);

        auto lowCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 300);
        lowCut[0].coefficients = lowCutCoef;
        lowCut[1].coefficients = lowCutCoef;
        lowCut[0].process(contextLeft);
        lowCut[1].process(contextRight);

        auto peak1Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 700, 1, db10);
        peak1[0].coefficients = peak1Coef;
        peak1[1].coefficients = peak1Coef;
        peak1[0].process(contextLeft);
        peak1[1].process(contextRight);

        auto peak2Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 2000, .5, juce::Decibels::decibelsToGain(-5.f));
        peak2[0].coefficients = peak2Coef;
        peak2[1].coefficients = peak2Coef;
        peak2[0].process(contextLeft);
        peak2[1].process(contextRight);

        auto peak3Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 6000, .3, juce::Decibels::decibelsToGain(5.f));
        peak3[0].coefficients = peak3Coef;
        peak3[1].coefficients = peak3Coef;
        peak3[0].process(contextLeft);
        peak3[1].process(contextRight);

        auto highCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighShelf   (sampleRate, 10000, .71, juce::Decibels::decibelsToGain(-5.f));
        highCut[0].coefficients = highCutCoef;
        highCut[1].coefficients = highCutCoef;
        highCut[0].process(contextLeft);
        highCut[1].process(contextRight);
    }


    else if (phone->get())
    {
        auto lowCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 800);
        lowCut[0].coefficients = lowCutCoef;
        lowCut[1].coefficients = lowCutCoef;
        lowCut[0].process(contextLeft);
        lowCut[1].process(contextRight);

        auto peak1Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 1000, .71, juce::Decibels::decibelsToGain(5.f));
        peak1[0].coefficients = peak1Coef;
        peak1[1].coefficients = peak1Coef;
        peak1[0].process(contextLeft);
        peak1[1].process(contextRight);

        auto peak2Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 3000, .71, juce::Decibels::decibelsToGain(8.f));
        peak2[0].coefficients = peak2Coef;
        peak2[1].coefficients = peak2Coef;
        peak2[0].process(contextLeft);
        peak2[1].process(contextRight);

        auto peak3Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 7500, .71, juce::Decibels::decibelsToGain(10.f));
        peak3[0].coefficients = peak3Coef;
        peak3[1].coefficients = peak3Coef;
        peak3[0].process(contextLeft);
        peak3[1].process(contextRight);

        auto highCutCoef = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 12000);
        highCut[0].coefficients = highCutCoef;
        highCut[1].coefficients = highCutCoef;
        highCut[0].process(contextLeft);
        highCut[1].process(contextRight);
        
    }

    else if (tv->get())
    {
        auto lowCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 100);
        lowCut[0].coefficients = lowCutCoef;
        lowCut[1].coefficients = lowCutCoef;
        lowCut[0].process(contextLeft);
        lowCut[1].process(contextRight);

        auto peak1Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 300, .71, juce::Decibels::decibelsToGain(10.f));
        peak1[0].coefficients = peak1Coef;
        peak1[1].coefficients = peak1Coef;
        peak1[0].process(contextLeft);
        peak1[1].process(contextRight);

        auto peak2Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 2250, .71, juce::Decibels::decibelsToGain(-2.5));
        peak2[0].coefficients = peak2Coef;
        peak2[1].coefficients = peak2Coef;;
        peak2[0].process(contextLeft);
        peak2[1].process(contextRight);

        auto peak3Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 10000, .71, juce::Decibels::decibelsToGain(-10.f));
        peak3[0].coefficients = peak3Coef;
        peak3[1].coefficients = peak3Coef;
        peak3[0].process(contextLeft);
        peak3[1].process(contextRight);

        auto highCutCoef = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 15000);
        highCut[0].coefficients = highCutCoef;
        highCut[1].coefficients = highCutCoef;
        highCut[0].process(contextLeft);
        highCut[1].process(contextRight);
    }

    else if (airpods->get())
    {
        float db5 = juce::Decibels::decibelsToGain(5.f);

        auto lowCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 100);
        lowCut[0].coefficients = lowCutCoef;
        lowCut[1].coefficients = lowCutCoef;
        lowCut[0].process(contextLeft);
        lowCut[1].process(contextRight);

        auto peak1Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 200, .4, juce::Decibels::decibelsToGain(2.5));
        peak1[0].coefficients = peak1Coef;
        peak1[1].coefficients = peak1Coef;
        peak1[0].process(contextLeft);
        peak1[1].process(contextRight);

        auto peak2Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 2000, .71, db5);
        peak2[0].coefficients = peak2Coef;
        peak2[1].coefficients = peak2Coef;
        peak2[0].process(contextLeft);
        peak2[1].process(contextRight);

        auto peak3Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 10000, 2, db5);
        peak3[0].coefficients = peak3Coef;
        peak3[1].coefficients = peak3Coef;
        peak3[0].process(contextLeft);
        peak3[1].process(contextRight);

        auto highCutCoef = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 15000);
        highCut[0].coefficients = highCutCoef;
        highCut[1].coefficients = highCutCoef;
        highCut[0].process(contextLeft);
        highCut[1].process(contextRight);
    }

    else
    {
        float db = juce::Decibels::decibelsToGain(-2.5);

        auto lowCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 60);
        lowCut[0].coefficients = lowCutCoef;
        lowCut[1].coefficients = lowCutCoef;
        lowCut[0].process(contextLeft);
        lowCut[1].process(contextRight);

        auto peak1Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 100, 2, juce::Decibels::decibelsToGain(-15.f));
        peak1[0].coefficients = peak1Coef;
        peak1[1].coefficients = peak1Coef;
        peak1[0].process(contextLeft);
        peak1[1].process(contextRight);

        auto peak2Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 300, .4, juce::Decibels::decibelsToGain(2.5));
        peak2[0].coefficients = peak2Coef;
        peak2[1].coefficients = peak2Coef;
        peak2[0].process(contextLeft);
        peak2[1].process(contextRight);

        auto peak3Coef = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 2500, .3, db);
        peak3[0].coefficients = peak3Coef;
        peak3[1].coefficients = peak3Coef;
        peak3[0].process(contextLeft);
        peak3[1].process(contextRight);

        auto highCutCoef = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 10000, .71, db);
        highCut[0].coefficients = highCutCoef;
        highCut[1].coefficients = highCutCoef;
        highCut[0].process(contextLeft);
        highCut[1].process(contextRight);
    }
}

//==============================================================================
bool QwikRefAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* QwikRefAudioProcessor::createEditor()
{
    return new QwikRefAudioProcessorEditor (*this);
}

//==============================================================================
void QwikRefAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void QwikRefAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout QwikRefAudioProcessor::createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<AudioParameterBool>("car", "Car", true));
    layout.add(std::make_unique<AudioParameterBool>("laptop", "Laptop", false));
    layout.add(std::make_unique<AudioParameterBool>("phone", "Phone", false));
    layout.add(std::make_unique<AudioParameterBool>("tv", "TV", false));
    layout.add(std::make_unique<AudioParameterBool>("airpods", "Airpods", false));
    layout.add(std::make_unique<AudioParameterBool>("btSpeaker", "BT Speaker", false));
    layout.add(std::make_unique<AudioParameterBool>("power", "Power", true));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QwikRefAudioProcessor();
}
