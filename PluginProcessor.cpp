/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
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
    
    //streamline making parameter rows
    addParameter(speed = new juce::AudioParameterFloat("speed", "-Speed", 0.0, 1.0, 0.5));
    addParameter(velocity = new juce::AudioParameterInt("velo", "-Velocity", 0, 150, 1));
    addParameter(sync = new juce::AudioParameterBool("sync", "bBPM Link",true));
    addParameter(rest = new juce::AudioParameterBool("rest", "-Include Rests",true));
    addParameter(latch = new juce::AudioParameterBool("latch", "-Latch",false));
    addParameter(octaves = new juce::AudioParameterInt("octaves", "iOctaveCount", 1, 5, 1)); 
    addParameter(direction = new juce::AudioParameterChoice("direction", "-Direction", {"Up","Down","Random"}, 0));

    // steps could be auto (number of notes in chord) or manual; if (random)? auto : manual
    // if (steps > notes.size : steps = notes.size)
    // if (random) : rests option active
    //addParameter(steps = new juce::AudioParameterInt("Steps", "steps", 0, 5, 0));
}

NewProjectAudioProcessor::~NewProjectAudioProcessor()
{
}

//==============================================================================
const juce::String NewProjectAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewProjectAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NewProjectAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NewProjectAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NewProjectAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewProjectAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NewProjectAudioProcessor::getProgramName (int index)
{
    return {};
}

void NewProjectAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NewProjectAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    notes.clear();                          // [1]
    currentNote = 0;                        // [2]
    lastNoteValue = -1;                     // [3]
    time = 0;                               // [4]
    tempo = 112;
    rate = static_cast<float> (sampleRate); // [5]
}

void NewProjectAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
   
    // the audio buffer in a midi effect will have zero channels!
    // but we need an audio buffer to getNumSamples....so....this next line will stay commented
    //jassert(buffer.getNumChannels() == 0);                                                         // [6]

    // however we use the buffer to get timing information
    auto numSamples = buffer.getNumSamples();                                                       // [7]

    juce::MidiBuffer processedMidi;

    juce::MidiMessage m;
    
    if(getPlayHead() != nullptr)
        getPlayHead()->getCurrentPosition(murr);
        tempo = murr.bpm;
        numerator = murr.timeSigNumerator;

    // get note duration
        syncSpeed = 1/std::pow(2.0f,(*speed * 100.0f) - 90.0f);
        auto noteDuration = (*sync) ?
            static_cast<int> (std::ceil(rate * 0.25f * (0.1f + (1.0f - (*speed)))))
            : static_cast<int> (std::ceil(rate * 0.25f * (tempo/60)*numerator*syncSpeed)); // should correspond to one quarter note
    //test = (*sync) ? noteDuration : 100;
            
    for (const auto metadata : midi)                                                                // [9]
    {
        const auto msg = metadata.getMessage();

        if      (msg.isNoteOn())  notes.add (msg.getNoteNumber());
        else if (msg.isNoteOff()) notes.removeValue (msg.getNoteNumber());
    }

    midi.clear();                                                                                   // [10]

    if ((time + numSamples) >= noteDuration)                                                        // [11]
    {
        auto offset = juce::jmax (0, juce::jmin ((int) (noteDuration - time), numSamples - 1));     // [12]

        if (lastNoteValue > 0)                                                                      // [13]
        {
            processedMidi.addEvent (juce::MidiMessage::noteOff (1, lastNoteValue), offset);
            lastNoteValue = -1;
        }

        if (notes.size() > 0)                                                                       // [14]
        {
            currentNote = (currentNote + 1) % notes.size();
            lastNoteValue = notes[currentNote];
            processedMidi.addEvent (juce::MidiMessage::noteOn  (1, lastNoteValue, (juce::uint8) 80), offset);
        }

    }

    time = (time + numSamples) % noteDuration;                                                      // [15]

    //always use swapWith(), avoids unpredictable behavior from directly editing midi buffer
    midi.swapWith(processedMidi);
}

//==============================================================================
bool NewProjectAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new AarrowAudioProcessorEditor (*this);
}

//==============================================================================
void NewProjectAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream(destData, true).writeFloat(*speed);
    juce::MemoryOutputStream(destData, true).writeInt(*velocity);
    juce::MemoryOutputStream(destData, true).writeInt(*sync);
}

void NewProjectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    speed->setValueNotifyingHost(juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readFloat());
    velocity->setValueNotifyingHost(juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readInt());
    sync->setValueNotifyingHost(juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readBool());
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
