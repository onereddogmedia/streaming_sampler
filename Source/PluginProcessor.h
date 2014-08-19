/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic startup code for a Juce application.

  ==============================================================================
*/

#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include "StreamingSampler.h"


// Enter the path to a valid sample file (stereo wave) here
String path("C://piano.wav");

// Set this to 1 to see the disk usage in action. It naively calls DBG() in every processBlock, so
// only set this to 1 to see how it works and then use StreamingSamplerVoice::getDiskUsage() in a
// correct way
#define DEBUG_DISK_USAGE 0

//==============================================================================
/**
*/
class StreamingDemoAudioProcessor  : public AudioProcessor
{
public:
    //==============================================================================
    StreamingDemoAudioProcessor();
    ~StreamingDemoAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor* createEditor();
    bool hasEditor() const;

    //==============================================================================
    const String getName() const;

    int getNumParameters();

    float getParameter (int index);
    void setParameter (int index, float newValue);

	const String getParameterName (int index);
	const String getParameterText (int index);

	const String getInputChannelName (int channelIndex) const;
    const String getOutputChannelName (int channelIndex) const;
    bool isInputChannelStereoPair (int index) const;
    bool isOutputChannelStereoPair (int index) const;

    bool acceptsMidi() const;
    bool producesMidi() const;
    bool silenceInProducesSilenceOut() const;
    double getTailLengthSeconds() const;

    //==============================================================================
    int getNumPrograms();
    int getCurrentProgram();
    void setCurrentProgram (int index);
    const String getProgramName (int index);
    void changeProgramName (int index, const String& newName);

    //==============================================================================
    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);

private:

	// The Synthesiser that will play the streaming sounds;
	Synthesiser synth;

	// The ThreadPool that will manage the background reading
	ScopedPointer<ThreadPool> backgroundThread;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StreamingDemoAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
