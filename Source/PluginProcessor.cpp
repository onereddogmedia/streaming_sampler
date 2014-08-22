/*
  ==============================================================================

    StreamingSampler Demo

  ==============================================================================
*/

#include "PluginProcessor.h"

//==============================================================================
StreamingDemoAudioProcessor::StreamingDemoAudioProcessor():
	backgroundThread(new ThreadPool())
{
	// Make a simple key map for the sound
	BigInteger map;
	map.setRange(0, 127, false);
	map.setRange(48, 24, true);

	try
	{
		// Add the sampler sound to the synth
		synth.addSound(new StreamingSamplerSound(File(path), map, 60));
	}
	catch(LoadingError error)
	{
		// print the error message if something went wrong
		DBG("Error loading: " + error.fileName + ": " + error.errorDescription);

		jassertfalse;
		return;
	}

	// Uncomment this to load everything into memory
	//dynamic_cast<StreamingSamplerSound*>(synth.getSound(0))->loadEntireSample();

	for(int i = 0; i < 4; ++i)
	{
		// Add a sampler voice and pass the background thread
		synth.addVoice(new StreamingSamplerVoice(backgroundThread));
	}
}

StreamingDemoAudioProcessor::~StreamingDemoAudioProcessor()
{
	synth.clearSounds();
	synth.clearVoices();
}

void StreamingDemoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	synth.setCurrentPlaybackSampleRate(sampleRate);

	for(int i = 0; i < synth.getNumVoices(); i++)
	{
		StreamingSamplerVoice *v = dynamic_cast<StreamingSamplerVoice*>(synth.getVoice(i));

		// You have to call prepareToPlay for each SamplerVoice since it initializes its internal buffers.
		v->prepareToPlay(sampleRate, samplesPerBlock);

		// This sets the buffer size of the internal stream buffers so that it loads
		// new data about every 32 blocks.
		v->setLoaderBufferSize(samplesPerBlock * 32);
	}
}

void StreamingDemoAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
	buffer.clear();

	// Renders everything
	synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
	
#if DEBUG_DISK_USAGE

	double usage = 0.0;

	for(int i = 0; i < synth.getNumVoices(); i++)
	{
		usage += dynamic_cast<StreamingSamplerVoice*>(synth.getVoice(i))->getDiskUsage();
	}

	DBG("Disk usage: " + String(usage, 3));

#endif
    
}

// Commencing stupid methods

void StreamingDemoAudioProcessor::releaseResources() {}
bool StreamingDemoAudioProcessor::hasEditor() const { return false; }
AudioProcessorEditor* StreamingDemoAudioProcessor::createEditor() { return nullptr; }
void StreamingDemoAudioProcessor::getStateInformation (MemoryBlock& /*destData*/) { }  
void StreamingDemoAudioProcessor::setStateInformation (const void* /*data*/, int /*sizeInBytes*/) {}
//==============================================================================
const String StreamingDemoAudioProcessor::getName() const {    return JucePlugin_Name; }
int StreamingDemoAudioProcessor::getNumParameters() {    return 0; }
float StreamingDemoAudioProcessor::getParameter (int /*index*/) { return 0.0f; }
void StreamingDemoAudioProcessor::setParameter (int /*index*/, float /*newValue*/) { }
const String StreamingDemoAudioProcessor::getParameterName (int /*index*/) {    return String::empty; }
const String StreamingDemoAudioProcessor::getParameterText (int /*index*/) {    return String::empty; }
const String StreamingDemoAudioProcessor::getInputChannelName (int channelIndex) const {    return String (channelIndex + 1); }
const String StreamingDemoAudioProcessor::getOutputChannelName (int channelIndex) const {    return String (channelIndex + 1); }
bool StreamingDemoAudioProcessor::isInputChannelStereoPair (int /*index*/) const { return true; }
bool StreamingDemoAudioProcessor::isOutputChannelStereoPair (int /*index*/) const {    return true; }
bool StreamingDemoAudioProcessor::acceptsMidi() const {   return true; }
bool StreamingDemoAudioProcessor::producesMidi() const{   return false; }
bool StreamingDemoAudioProcessor::silenceInProducesSilenceOut() const {    return false;}
double StreamingDemoAudioProcessor::getTailLengthSeconds() const{    return 0.0;}
int StreamingDemoAudioProcessor::getNumPrograms(){    return 1;}
int StreamingDemoAudioProcessor::getCurrentProgram(){    return 0;}
void StreamingDemoAudioProcessor::setCurrentProgram (int /*index*/){}
const String StreamingDemoAudioProcessor::getProgramName (int /*index*/){    return String::empty;}
void StreamingDemoAudioProcessor::changeProgramName (int /*index*/, const String& /*newName*/){}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StreamingDemoAudioProcessor();
}

// End of stupid methods