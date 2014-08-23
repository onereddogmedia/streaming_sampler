streaming_sampler
=================

A extension to JUCE's Sampler class which uses MemoryMappedAudioFormatReader to stream the samples directly from disk.

It preloads the start of the sample when the sound is loaded and starts filling intermediate buffers in a background thread when the sound is played back.

Known limitations:

- .wav file support only (will add .aiff later)
- stereo support only
- no resampling ( will be added in upcoming version)

It comes with an example plugin project that shows the usage of this class.

See the doxygen generated API for further documentation
