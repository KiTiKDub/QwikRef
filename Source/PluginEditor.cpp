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

    juce::PropertiesFile::Options options;
    options.applicationName = "QwikRef";
    options.commonToAllUsers = true;
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters(options);

    if (auto *constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio(static_cast<double>(orgWidth) / static_cast<double>(orgHeight));
        constrainer->setSizeLimits(orgWidth, orgHeight,
                                   orgWidth * 4, orgHeight * 4);
    }

    auto sizeRatio{1.0};
    if (auto *properties = appProperties.getCommonSettings(true))
    {
        sizeRatio = properties->getDoubleValue("sizeRatio", 1.0);
    }

    setResizable(true, true);
    setSize(static_cast<int>(orgWidth * sizeRatio),
            static_cast<int>(orgHeight * sizeRatio));
}

QwikRefAudioProcessorEditor::~QwikRefAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void QwikRefAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto sizeRatio{1.0};
    if (auto *properties = appProperties.getCommonSettings(true))
    {
        sizeRatio = properties->getDoubleValue("sizeRatio", 1.0);
    }

    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);

    auto bounds = juce::Rectangle<int>{orgWidth, orgHeight};

    auto infoArea = bounds.removeFromTop(30);
    infoArea.setWidth(infoArea.getWidth()*sizeRatio);
    infoArea.setHeight(infoArea.getHeight()*sizeRatio);
    auto logoBackdrop = bounds;

    auto top = bounds.removeFromTop(bounds.getHeight() * .33);
    auto mid = bounds.removeFromTop(bounds.getHeight() * .5);
    auto low = bounds;

    auto leftTop = top.removeFromLeft(top.getWidth() * .5);
    auto leftMid = mid.removeFromLeft(mid.getWidth() * .5);
    auto leftLow = low.removeFromLeft(low.getWidth() * .5);

    juce::Path divider;
    divider.startNewSubPath(0, 30 * sizeRatio);
    divider.lineTo(200 * sizeRatio, 30 * sizeRatio);
    g.strokePath(divider, juce::PathStrokeType(1));

    logoBackdrop.setWidth(logoBackdrop.getWidth() * sizeRatio);
    logoBackdrop.setHeight(logoBackdrop.getHeight() * sizeRatio);
    logoBackdrop.setTop(30*sizeRatio);
    logoBackdrop.setBottom(orgHeight * sizeRatio);

    auto newFont = juce::Font(juce::Typeface::createSystemTypefaceFor(BinaryData::offshore_ttf, BinaryData::offshore_ttfSize));

    newFont.setHeight(25*sizeRatio);
    g.setFont(newFont);
    g.drawFittedText("QwikRef", infoArea.toNearestInt(), juce::Justification::Justification::centred, 1);
    newFont.setHeight(12*sizeRatio);
    g.setFont(newFont);
    g.drawFittedText("By KiTiK Music", logoBackdrop.toNearestInt(), juce::Justification::Justification::centredTop, 1);
    
    auto logo = juce::ImageCache::getFromMemory(BinaryData::KITIK_LOGO_NO_BKGD_png, BinaryData::KITIK_LOGO_NO_BKGD_pngSize); 
    g.setOpacity(.3f);
    g.drawImage(logo, logoBackdrop.toFloat(), juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1);
}

void QwikRefAudioProcessorEditor::resized()
{
    const auto scaleFactor = static_cast<float>(getWidth()) / orgWidth;
    if (auto *properties = appProperties.getCommonSettings(true))
    {
        properties->setValue("sizeRatio", scaleFactor);
    }

    //auto bounds = getLocalBounds();
    auto bounds = juce::Rectangle<int>{orgWidth, orgHeight};

    auto infoArea = bounds.removeFromTop(30);
    auto powerArea = infoArea.removeFromLeft(infoArea.getWidth() * .15);
    auto linkSpace = infoArea.removeFromRight(infoArea.getWidth() * .2);

    auto top = bounds.removeFromTop(bounds.getHeight() * .33);
    auto mid = bounds.removeFromTop(bounds.getHeight() * .5);
    auto low = bounds;

    auto leftTop = top.removeFromLeft(top.getWidth() * .5);
    auto leftMid = mid.removeFromLeft(mid.getWidth() * .5);
    auto leftLow = low.removeFromLeft(low.getWidth() * .5);

    car.setTransform(juce::AffineTransform::scale(scaleFactor));
    laptop.setTransform(juce::AffineTransform::scale(scaleFactor));
    phone.setTransform(juce::AffineTransform::scale(scaleFactor));
    tv.setTransform(juce::AffineTransform::scale(scaleFactor));
    airpods.setTransform(juce::AffineTransform::scale(scaleFactor));
    speaker.setTransform(juce::AffineTransform::scale(scaleFactor));
    power.setTransform(juce::AffineTransform::scale(scaleFactor));
    gumroad.setTransform(juce::AffineTransform::scale(scaleFactor));

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
