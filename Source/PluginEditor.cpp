/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
QwikRefAudioProcessorEditor::QwikRefAudioProcessorEditor (QwikRefAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    carAT(p.apvts, "car", car), laptopAT(p.apvts, "laptop", laptop),
    phoneAT(p.apvts, "phone", phone), tvAT(p.apvts, "tv", tv),
    airpodsAT(p.apvts, "airpods", airpods), speakerAT(p.apvts, "btSpeaker", speaker),
    powerAT(p.apvts, "power", power)
{

    setLookAndFeel(&lnf);

    car.setRadioGroupId(1, juce::NotificationType::dontSendNotification);
    laptop.setRadioGroupId(1, juce::NotificationType::dontSendNotification);
    phone.setRadioGroupId(1, juce::NotificationType::dontSendNotification);
    tv.setRadioGroupId(1, juce::NotificationType::dontSendNotification);
    airpods.setRadioGroupId(1, juce::NotificationType::dontSendNotification);
    speaker.setRadioGroupId(1, juce::NotificationType::dontSendNotification);

    car.setButtonText("Car");
    laptop.setButtonText("Laptop");
    phone.setButtonText("Phone");
    tv.setButtonText("TV");
    airpods.setButtonText("Airpods");
    speaker.setButtonText("BT Speaker");

    power.setComponentID("Power");

    addAndMakeVisible(car);
    addAndMakeVisible(laptop);
    addAndMakeVisible(phone);
    addAndMakeVisible(tv);
    addAndMakeVisible(airpods);
    addAndMakeVisible(speaker);
    addAndMakeVisible(power);

    addAndMakeVisible(gumroad);

    setSize (200, 230);
}

QwikRefAudioProcessorEditor::~QwikRefAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void QwikRefAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);

    auto bounds = getLocalBounds();

    auto infoArea = bounds.removeFromTop(30);
    auto logoBackdrop = bounds;

    auto top = bounds.removeFromTop(bounds.getHeight() * .33);
    auto mid = bounds.removeFromTop(bounds.getHeight() * .5);
    auto low = bounds;

    auto leftTop = top.removeFromLeft(top.getWidth() * .5);
    auto leftMid = mid.removeFromLeft(mid.getWidth() * .5);
    auto leftLow = low.removeFromLeft(low.getWidth() * .5);

    juce::Path divider;
    divider.startNewSubPath(0, 30);
    divider.lineTo(200, 30);
    g.strokePath(divider, juce::PathStrokeType(1));

    juce::Font::findFonts(fonts);
    auto test = fonts[56];
    test.setItalic(true);
    test.setHeight(25);

    g.setFont(test);
    g.drawFittedText("QwikRef", infoArea.toNearestInt(), juce::Justification::Justification::centred, 1);
    g.setFont(12);
    g.drawFittedText("By KiTiK Music", logoBackdrop.toNearestInt(), juce::Justification::Justification::centredTop, 1);
    
    auto logo = juce::ImageCache::getFromMemory(BinaryData::KITIK_LOGO_NO_BKGD_png, BinaryData::KITIK_LOGO_NO_BKGD_pngSize); 
    g.setOpacity(.3);
    g.drawImage(logo, logoBackdrop.toFloat(), juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1);
}

void QwikRefAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto infoArea = bounds.removeFromTop(30);
    auto powerArea = infoArea.removeFromLeft(infoArea.getWidth() * .15);
    auto linkSpace = infoArea.removeFromRight(infoArea.getWidth() * .2);

    auto top = bounds.removeFromTop(bounds.getHeight() * .33);
    auto mid = bounds.removeFromTop(bounds.getHeight() * .5);
    auto low = bounds;

    auto leftTop = top.removeFromLeft(top.getWidth() * .5);
    auto leftMid = mid.removeFromLeft(mid.getWidth() * .5);
    auto leftLow = low.removeFromLeft(low.getWidth() * .5);

    car.setBounds(leftTop);
    laptop.setBounds(top);
    phone.setBounds(leftMid);
    tv.setBounds(mid);
    airpods.setBounds(leftLow);
    speaker.setBounds(low);
    power.setBounds(powerArea);

    auto font = juce::Font::Font(10);
    gumroad.setFont(font, false);
    gumroad.setColour(0x1001f00, juce::Colours::white);
    gumroad.setBounds(linkSpace);
}
