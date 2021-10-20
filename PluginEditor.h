/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class AarrowAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    AarrowAudioProcessorEditor (NewProjectAudioProcessor&);
    ~AarrowAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    // This constructor has been changed to take a reference instead of a pointer
    //JUCE_DEPRECATED_WITH_BODY(AarrowAudioProcessorEditor(juce::AudioProcessor* p), : AarrowAudioProcessorEditor(*p) {})
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    NewProjectAudioProcessor& audioProcessor;

    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AarrowAudioProcessorEditor)
};
