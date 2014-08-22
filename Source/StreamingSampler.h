/**
  ==============================================================================

	@mainpage StreamingSampler class
    @author Christoph Hart
	@version 1.0

	A Sampler subclass which uses MemoryMappedAudioFormatReader to stream the samples
	directly from disk.

	It preloads the start of the sample when the sound is loaded and starts filling 
	intermediate buffers in a background thread when the sound is played back.

	Known limitations:

	- .wav file support only (will add .aiff later)
	- stereo support only
	- no resampling ( will be added in upcoming version)

	It comes with an example plugin project that shows the usage of this class.

	@copyright

	This class is published under the MIT license, but you need of course a valid 
	JUCE license if you want to go closed source.

	See http://juce.com

	Copyright (c) 2014 Christoph Hart

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

  ==============================================================================
*/

#ifndef STREAMINGSAMPLER_H_INCLUDED
#define STREAMINGSAMPLER_H_INCLUDED

// This class is a spin off of my upcoming sampler framework, so in order to use it in another project, leave this at '1'
#define STANDALONE 1

#if STANDALONE
#include <JuceHeader.h>
#endif

// This is the maximum value for sample pitch manipulation (this means 3 octaves, which should be more than enough
#define MAX_SAMPLER_PITCH 8

// This is the default preload size. I defined a pretty random value here, but you can change this dynamically.
#define PRELOAD_SIZE 11000

// Same as the preload size.
#define BUFFER_SIZE_FOR_STREAM_BUFFERS 11000

// You can set this to 0, if you want to disable background threaded reading. The files will then be read directly in the audio thread,
// which is not the smartest thing to do, but it comes to good use for debugging.
#define USE_BACKGROUND_THREAD 1

// By default, every voice adds its output to the supplied buffer. Depending on your architecture, it could be more practical to
// set (overwrite) the buffer. In this case, set this to 1.
#if STANDALONE
#define OVERWRITE_BUFFER_WITH_VOICE_DATA 0
#else
#define OVERWRITE_BUFFER_WITH_VOICE_DATA 1
#endif

/** An object of this class will be thrown if the loading of the sound fails.
*/
struct LoadingError
{
	/** Create one of this, if the sound fails to load.
	*
	*	@param fileName the file that caused the error.
	*	@param errorDescription a description of what went wrong.
	*/
	LoadingError(const String &fileName_, const String &errorDescription_):
		fileName(fileName_),
		errorDescription(errorDescription_)
	{};

	String fileName;
	String errorDescription;
};

/** A SamplerSound which provides buffered disk streaming using memory mapped file access and a preloaded sample start. */
class StreamingSamplerSound: public SynthesiserSound
{
public:

	/** Creates a new StreamingSamplerSound.
	*
	*	@param fileToLoad a stereo wave file that is read as memory mapped file.
	*	@param midiNotes the note map
	*	@param midiNoteForNormalPitch the root note
	*/
	StreamingSamplerSound(const File &fileToLoad, BigInteger midiNotes, int midiNoteForNormalPitch);

	/** Checks if the note is mapped to the supplied note number. */
	bool appliesToNote(const int midiNoteNumber) override { return midiNotes[midiNoteNumber]; };

	/** Always returns true ( can be implemented if used, but I don't need it) */
	bool appliesToChannel(const int midiChannel) override {return true;};

	/** Returns the pitch factor for the note number. */
	double getPitchFactor(int noteNumberToPitch) const { return pow(2.0, (noteNumberToPitch - rootNote) / 12.0); };

	/** Set the preload size. 
	*
	*	You can also tell the sound to load everything into memory by calling loadEntireSample()
	*/
	void setPreloadSize(int newPreloadSizeInSamples);

	/** Tell the sound to load everything into memory. */
	void loadEntireSample() {setPreloadSize(-1);};

	/** Returns the size of the preload buffer in bytes. You can use this method to check how much memory the sound uses. */
	size_t getActualPreloadSize() const
	{
		return (size_t)(preloadSize *preloadBuffer.getNumChannels()) * sizeof(float);
	}

	/** Gets the sound into active memory.
	*
	*	This is a wrapper around MemoryMappedAudioFormatReader::touchSample(), and I didn't check if it is necessary. 
	*/
	void wakeSound() { memoryReader->touchSample(0); };

	/** Checks if the file is mapped and has enough samples.
	*
	*	Call this before you call fillSampleBuffer() to check if the audio file has enough samples.
	*/
	bool hasEnoughSamplesForBlock(int64 maxSampleIndexInFile) const;

	/** Returns read only access to the preload buffer.
	*
	*	This is used by the SampleLoader class to fetch the samples from the preloaded buffer until the disk streaming
	*	thread fills the other buffer.
	*/
	const AudioSampleBuffer &getPreloadBuffer() const {return preloadBuffer;};


	/** The wave file that contains the sample data. It is assumed to be stereo and 44.1kHz 
	*
	*	This file will be memory mapped and read from during playback by a StreamingSamplerVoice and its SamplerLoader
	*/
	const String fileName;

	/** The root note of the sample. If the sample is pitched, this note number plays back the sample with the 
		original samplerate, but there is a limit of three octaves up to protect the streaming (I can't think of 
		a musical useful purpose of transposing a sound more than 3 octaves, but you can change SAMPLER_MAX_PITCH
		to allow larger values. */
	int rootNote;

	/** The note mapping of the sound (same functionality as SamplerSound) */
	BigInteger midiNotes;
	
private:

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StreamingSamplerSound)

	/** This fills the supplied AudioSampleBuffer with samples.
	*
	*	It copies the samples either from the preload buffer or reads it directly from the file, so don't call this method from the 
	*	audio thread, but use the SampleLoader class which handles the background thread stuff.
	*/
	void fillSampleBuffer(AudioSampleBuffer &sampleBuffer, int samplesToCopy, int uptime) const;

	friend class SampleLoader;

	AudioSampleBuffer preloadBuffer;	
	double sampleRate;
	ScopedPointer<MemoryMappedAudioFormatReader> memoryReader;

	int preloadSize;

};

/** This is a utility class that handles buffered sample streaming in a background thread.
*
*	It is derived from ThreadPoolJob, so whenever you want it to read new samples, add an instance of this 
*	to a ThreadPool (but don't delete it!) and it will do what it is supposed to do.
*/
class SampleLoader: public ThreadPoolJob
{
public:

	/** Creates a new SampleLoader.
	*
	*	Normally you don't need to call this manually, as a StreamingSamplerVoice automatically creates a instance as member.
	*/
	SampleLoader(ThreadPool *pool_):
		ThreadPoolJob("SampleLoader"),
		backgroundPool(pool_),
		sound(nullptr),
		readIndex(0),
		positionInSampleFile(0),
		writeBufferIsBeingFilled(false),
		diskUsage(0.0)
	{
		setBufferSize(BUFFER_SIZE_FOR_STREAM_BUFFERS);
	};

	/** Sets the buffer size in samples. */
	void setBufferSize(int newBufferSize);

	/** This fills the currently inactive buffer with samples from the SamplerSound.
	*
	*	The write buffer will be locked for the time of the read operation. Also it measures the time for getDiskUsage();
	*/
	JobStatus runJob() override;

	/** Fills a AudioSampleBuffer with samples from the current read buffer.
	*
	*	It uses two internal buffers. If the active buffer 'A' is completely read, it swaps the buffers, continues reading from buffer 'B' and returns true, so you can 
	*	run the background thread that fills the buffer 'A' with new samples.
	*
	*	@param sampleBlockBuffer the buffer that will be filled.
	*	@param numSamples the expected amount of samples that is likely to be used in the current processBlock method.
	*					  This number doesn't need to be exact (you can ask for more samples than you actually need),
	*	@param sampleIndex the index in the sample file. This acts as the exact "clock" variable (unlike numSamples), so make sure
						   you supply the right value here, or it will stutter pretty ugly!
	*/
	void fillSampleBlockBuffer(AudioSampleBuffer &sampleBlockBuffer, int numSamples, int sampleIndex);
	
	/** Call this whenever a sound was started.
	*
	*	This will set the read pointer to the preload buffer of the StreamingSamplerSound and start the background reading.
	*/
	void startNote(StreamingSamplerSound const *s);

	/** Returns the loaded sound. */
	const StreamingSamplerSound *getLoadedSound() const { return sound;	};

	/** Resets the loader (unloads the sound). */
	void reset()
	{
		ScopedLock sl(lock);
		sound = nullptr;
		diskUsage = 0.0;
	}

	/** Calculates and returns the disk usage.
	*
	*	It measures the time the background thread needed for the loading operation and divides it with the duration since the last
	*	call to requestNewData().
	*/
	double getDiskUsage() noexcept
	{
		const double returnValue = diskUsage;
		diskUsage = 0.0;
		return returnValue;
	};

private:

	// ============================================================================================ internal methods

	void requestNewData();
	
	bool swapBuffers();

	void fillInactiveBuffer();

	// ============================================================================================ member variables

	/** The class tries to be as lock free as possible (it only locks the buffer that is filled 
	*	during the read operation, but I have to lock everything for a few calls, so that's why
	*	there is a critical section.	
	*/
	CriticalSection lock;

	/** A simple mutex for the buffer that is being used for loading. */
	bool writeBufferIsBeingFilled;

	// variables for handling of the internal buffers

	StreamingSamplerSound const *sound;
	int readIndex;
	int bufferSize;
	int64 positionInSampleFile;
	AudioSampleBuffer const *readBuffer;
	AudioSampleBuffer *writeBuffer;

	double lastPosition;

	// variables for disk usage measurement

	double diskUsage;
	double lastCallToRequestData;

	// just a pointer to the used pool
	ThreadPool *backgroundPool;

	// the internal buffers

	AudioSampleBuffer b1, b2;
};

/** A SamplerVoice that streams the data from a StreamingSamplerSound
*
*	It uses a SampleLoader object to fetch the data and copies the values into an internal buffer, so you
*	don't have to bother with the SampleLoader's internals.
*/
class StreamingSamplerVoice: public SynthesiserVoice
{
public:
	StreamingSamplerVoice(ThreadPool *backgroundThreadPool);
	
	~StreamingSamplerVoice() {};

	/** Always returns true. */
	bool canPlaySound (SynthesiserSound*) { return true; };

	/** starts the streaming of the sound. */
	void startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/) override;
	
	const StreamingSamplerSound *getLoadedSound()
	{
		return loader.getLoadedSound();
	}

	void setLoaderBufferSize(int newBufferSize)
	{
		loader.setBufferSize(newBufferSize);
	};

	/** Clears the note data and resets the loader. */
	void stopNote (bool /*allowTailOff*/)
	{ 
		clearCurrentNote();
		loader.reset();
	};

	/** Adds it's output to the outputBuffer. */
	void renderNextBlock(AudioSampleBuffer &outputBuffer, int startSample, int numSamples) override;

	/** You can pass a pointer with float values containing pitch information for each sample.
	*
	*	The array size should be exactly the number of samples that are calculated in the current renderNextBlock method.
	*/
	void setPitchValues(const float *pitchDataForBlock)	{ pitchData = pitchDataForBlock; };

	/** Returns the disk usage of the voice. 
	*
	*	To get the disk usage of all voices, simply iterate over the voice list and add all disk usages.
	*/
	double getDiskUsage() {	return loader.getDiskUsage(); };

	/** Initializes its sampleBuffer. You have to call this manually, since there is no base class function. */
	void prepareToPlay(double sampleRate, int samplesPerBlock)
	{
		if(sampleRate != -1.0)
		{
			samplesForThisBlock = AudioSampleBuffer(2, samplesPerBlock * MAX_SAMPLER_PITCH);
			samplesForThisBlock.clear();
		}
	}

	/** Not implemented */
	virtual void controllerMoved(int /*controllerNumber*/, int /*controllerValue*/) override { };

	/** Not implemented */
	virtual void pitchWheelMoved(int /*pitchWheelValue*/) override { };

	/** resets everything. */
	void resetVoice()
	{
		voiceUptime = 0.0;
		uptimeDelta = 0.0;
		clearCurrentNote();
	};

private:

	const float *pitchData;

	// This lets the wrapper class access the internal data without annoying get/setters
	friend class ModulatorSamplerVoice; 

	double voiceUptime;
	double uptimeDelta;

	AudioSampleBuffer samplesForThisBlock;

	SampleLoader loader;
};

#endif  // STREAMINGSAMPLER_H_INCLUDED
