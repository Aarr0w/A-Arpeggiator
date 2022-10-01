/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class AarrowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AarrowLookAndFeel()
    {
        setColour(juce::Label::textColourId, juce::Colours::cyan);
        setColour(juce::Slider::thumbColourId, juce::Colours::antiquewhite);
        setColour(juce::Slider::trackColourId, (juce::Colours::cyan).withBrightness(0.8)); // You should really change the saturation on this..        setColour(juce::Label::textColourId, juce::Colours::cyan);

        setColour(juce::TextButton::textColourOnId, juce::Colours::cyan);
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);


        setColour(juce::ToggleButton::tickColourId, juce::Colours::orange);


    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AarrowLookAndFeel)
};


class AarrowAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AarrowAudioProcessorEditor(NewProjectAudioProcessor&);
    ~AarrowAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

    // This constructor has been changed to take a reference instead of a pointer
    //JUCE_DEPRECATED_WITH_BODY(AarrowAudioProcessorEditor(juce::AudioProcessor* p), : AarrowAudioProcessorEditor(*p) {})
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NewProjectAudioProcessor& audioProcessor;
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;
    AarrowLookAndFeel Aalf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AarrowAudioProcessorEditor)
};
