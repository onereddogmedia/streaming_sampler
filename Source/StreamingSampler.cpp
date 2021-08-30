/*
  =====================================================================================================

    StreamingSampler.cpp
    Created: 13 Aug 2014 8:51:30pm
    Author:  Christoph Hart

  =====================================================================================================
*/

#include "StreamingSampler.h"

// ==================================================================================================== StreamingSamplerSound methods

StreamingSamplerSound::StreamingSamplerSound(const File &fileToLoad, 
											 BigInteger midiNotes_, 
											 int midiNoteForNormalPitch):
	fileName(fileToLoad.getFullPathName()),
	midiNotes(midiNotes_),
	rootNote(midiNoteForNormalPitch)
{
	WavAudioFormat waf;
	memoryReader = waf.createMemoryMappedReader(fileToLoad);
	
	if(memoryReader != nullptr) memoryReader->mapEntireFile();

	else throw LoadingError(fileName, "file does not exist");
	
	if( ! memoryReader->getMappedSection().isEmpty() )
	{
		sampleRate = memoryReader->sampleRate;

		setPreloadSize(PRELOAD_SIZE);
	}
	else
	{
		throw LoadingError(fileName, "Error at memory mapping");
	}
}

void StreamingSamplerSound::setPreloadSize(int newPreloadSize)
{
	preloadSize = newPreloadSize;

	int64 maxSize = memoryReader->getMappedSection().getLength();

	if(newPreloadSize == -1 || preloadSize > maxSize)
	{
		preloadSize = (int)maxSize;
	};

	try
	{
		preloadBuffer = AudioSampleBuffer(2, preloadSize);
	}
	catch(std::bad_alloc memoryExeption)
	{
		throw LoadingError(fileName, "out of Memory!");
	}

	memoryReader->read(&preloadBuffer, 0, preloadSize, 0, true, true);
}

bool StreamingSamplerSound::hasEnoughSamplesForBlock(int64 maxSampleIndexInFile) const
{
	return maxSampleIndexInFile < memoryReader->getMappedSection().getEnd();
}

void StreamingSamplerSound::fillSampleBuffer(AudioSampleBuffer &sampleBuffer, int samplesToCopy, int uptime) const
{
	if(uptime + samplesToCopy < preloadSize)
	{
		FloatVectorOperations::copy(sampleBuffer.getWritePointer(0, 0), preloadBuffer.getReadPointer(0, uptime), samplesToCopy);
		FloatVectorOperations::copy(sampleBuffer.getWritePointer(1, 0), preloadBuffer.getReadPointer(1, uptime), samplesToCopy);
	}
	else 
	{
		memoryReader->read(&sampleBuffer, 0, samplesToCopy, uptime, true, true);
	}
};



// ==================================================================================================== SampleLoader methods

/** Sets the buffer size in samples. */
void SampleLoader::setBufferSize(int newBufferSize)
{
	bufferSize = newBufferSize;

	b1 = AudioSampleBuffer(2, bufferSize);
	b2 = AudioSampleBuffer(2, bufferSize);

	b1.clear();
	b2.clear();

	readBuffer = &b1;
	writeBuffer = &b2;

	reset();
}

void SampleLoader::startNote(StreamingSamplerSound const *s)
{
	ScopedLock sl(lock);

	diskUsage = 0.0;

	sound = s;
	readIndex = 0;

	// the read pointer will be pointing directly to the preload buffer of the sample sound
	readBuffer = &s->getPreloadBuffer();

	// If you hit this assert, you have to increase the buffer size of the preload buffer - it must be at least as big as
	// the streaming buffers.
	jassert(readBuffer->getNumSamples() >= bufferSize);

	writeBuffer = &b1;

	// Set the sampleposition to (1 * bufferSize) because the first buffer is the preload buffer
	positionInSampleFile = bufferSize;

	lastPosition = 0.0;


	// The other buffer will be filled on the next free thread pool slot
	if(!writeBufferIsBeingFilled)
	{
		requestNewData();
	}
};

void SampleLoader::fillSampleBlockBuffer(AudioSampleBuffer &sampleBlockBuffer, int numSamplesToCopy, int numSamplesToConsume, int sampleIndex)
{
	// Since the numSamples is only a estimate, the sampleIndex is used for the exact clock
	readIndex = sampleIndex % bufferSize;

	jassert(sound != nullptr);

	if(readIndex + numSamplesToCopy < bufferSize) // Copy all samples from the current read buffer
	{
		FloatVectorOperations::copy(sampleBlockBuffer.getWritePointer(0, 0), readBuffer->getReadPointer(0, readIndex), numSamplesToCopy);
		FloatVectorOperations::copy(sampleBlockBuffer.getWritePointer(1, 0), readBuffer->getReadPointer(1, readIndex), numSamplesToCopy);
	}

	else
	{
		// copy as much samples from current read buffer as possible
		const int remainingSamples = bufferSize - readIndex;
		jassert(remainingSamples <= numSamplesToCopy);
		FloatVectorOperations::copy(sampleBlockBuffer.getWritePointer(0, 0), readBuffer->getReadPointer(0, readIndex), remainingSamples);
		FloatVectorOperations::copy(sampleBlockBuffer.getWritePointer(1, 0), readBuffer->getReadPointer(1, readIndex), remainingSamples);

		// peek into write buffer for remaining samples
		jassert(!writeBufferIsBeingFilled); // fails when buffer is currently used by the background thread
		const int remainingSamplesInWriteBuffer = numSamplesToCopy - remainingSamples;
		FloatVectorOperations::copy(sampleBlockBuffer.getWritePointer(0, remainingSamples), writeBuffer->getReadPointer(0, 0), remainingSamplesInWriteBuffer);
		FloatVectorOperations::copy(sampleBlockBuffer.getWritePointer(1, remainingSamples), writeBuffer->getReadPointer(1, 0), remainingSamplesInWriteBuffer);

		// swap buffers if all samples from current read buffer have been consumed (avoid swapping buffers to early)
		if (readIndex + numSamplesToConsume >= bufferSize) {
			if (swapBuffers()) {
				positionInSampleFile += bufferSize;
				requestNewData();
			} else {
				jassertfalse; // fails when background thread was not quick enough -> increase preload / buffer size
			}
		}
	}
};


ThreadPoolJob::JobStatus SampleLoader::runJob()
{
	const double readStart = Time::highResolutionTicksToSeconds(Time::getHighResolutionTicks());

	fillInactiveBuffer();

	writeBufferIsBeingFilled = false;

	const double readStop = Time::highResolutionTicksToSeconds(Time::getHighResolutionTicks());
	const double readTime = (readStop - readStart);
	const double timeSinceLastCall = readStop - lastCallToRequestData;
	const double diskUsageThisTime =  readTime / timeSinceLastCall;
	diskUsage = jmax(diskUsage, diskUsageThisTime);
	lastCallToRequestData = readStart;

	return JobStatus::jobHasFinished;
}

void SampleLoader::requestNewData()
{
	writeBufferIsBeingFilled = true; // A poor man's mutex but gets the job done.

#if(USE_BACKGROUND_THREAD)

	// check if the background thread is already loading this sound
	jassert(! backgroundPool->contains(this));

	backgroundPool->addJob(this, false);
#else

	// run the thread job synchronously
	runJob();

#endif
};

void SampleLoader::fillInactiveBuffer()
{
	if(sound != nullptr && sound->hasEnoughSamplesForBlock(bufferSize + positionInSampleFile))
	{
		sound->fillSampleBuffer(*writeBuffer, bufferSize, (int)positionInSampleFile);
	}
};
	
bool SampleLoader::swapBuffers()
{
	if(readBuffer == &b1)
	{
		readBuffer = &b2;
		writeBuffer = &b1;
	}
	else // This condition will also be true if the read pointer points at the preload buffer
	{
		readBuffer = &b1;
		writeBuffer = &b2;
	}

	return writeBufferIsBeingFilled == false;
};

// ==================================================================================================== StreamingSamplerVoice methods

StreamingSamplerVoice::StreamingSamplerVoice(ThreadPool *pool):
loader(pool)
{
	pitchData = nullptr;
};

void StreamingSamplerVoice::startNote (int midiNoteNumber, 
									   float /*velocity*/, 
									   SynthesiserSound* s, 
									   int /*currentPitchWheelPosition*/)
{
	StreamingSamplerSound *sound = dynamic_cast<StreamingSamplerSound*>(s);

	loader.startNote(sound);

	jassert(sound != nullptr);
	sound->wakeSound();

	voiceUptime = 0.0;
	uptimeDelta = jmin(sound->getPitchFactor(midiNoteNumber), (double)MAX_SAMPLER_PITCH);
}


void StreamingSamplerVoice::renderNextBlock(AudioSampleBuffer &outputBuffer, int startSample, int numSamples)
{
	const StreamingSamplerSound *sound = loader.getLoadedSound();

	if(sound != nullptr)
	{

		const int pos = (int)voiceUptime;

		double numSamplesUsed = voiceUptime - pos;

		if(pitchData != nullptr)
		{
			for(int i = startSample; i < startSample + numSamples; ++i) 
			numSamplesUsed += jmin(uptimeDelta * pitchData[i], (double)MAX_SAMPLER_PITCH);
		}
		else
		{
			for(int i = startSample; i < startSample + numSamples; i++)
			numSamplesUsed += uptimeDelta;
		}

		const int samplesToCopy = (int)(numSamplesUsed) + 2; // get a few more for linear interpolating

		if( ! sound->hasEnoughSamplesForBlock(pos + samplesToCopy) )
		{
			resetVoice();
			return;
		}

		loader.fillSampleBlockBuffer(samplesForThisBlock, samplesToCopy, numSamplesUsed, pos);
	
		const float *inL = samplesForThisBlock.getReadPointer(0);
		const float *inR = samplesForThisBlock.getReadPointer(1);

		float *outL = outputBuffer.getWritePointer(0, startSample);
		float *outR = outputBuffer.getWritePointer(1, startSample);

		while (--numSamples >= 0)
		{
			const float indexFloat = (float)(voiceUptime - pos);
			const int index = (int)(indexFloat);

			jassert((index + 1) <= samplesToCopy);

			const float alpha = indexFloat - index;
			const float invAlpha = 1.0f - alpha;

			float l = inL[index] * invAlpha + inL[index+1] * alpha;
			float r = inR[index] * invAlpha + inR[index+1] * alpha;

#if OVERWRITE_BUFFER_WITH_VOICE_DATA
			*outL++ = l;
			*outR++ = r;
#else
			*outL++ += l;
			*outR++ += r;	
#endif
			voiceUptime += uptimeDelta * (pitchData == nullptr ? 1.0 : (double)pitchData[startSample]);
			++startSample;
		}
	}
};

// ==================================================================================================== StreamingSampler methods

