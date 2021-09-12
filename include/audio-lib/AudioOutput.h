#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <windows.h>

#include <cpthread/cpthread.h>
#include "wave.h"
#include "AudioSource.h"

#pragma comment(lib, "Winmm.lib")

#define BUFFER_SEGMENTS 10
#define BUFFER_FRACTION 100

enum AO_State {Playing, Paused, Stopped};

class AudioOutput
{
public:
	// Speaker device for playing the audio
	HWAVEOUT speaker;
	AO_State state;

	// Desired and supported format of the speaker device
	// If the speaker is not capable of supporting the desired format
	// The samples will be converted to the supported format
	WaveFmt desired_fmt;
	WaveFmt supported_fmt;

	char*   mixer_buffer;	// Buffer for mixing audio sources
	size_t  mixer_size;		// Block size of the mixer buffer
	char*   audio_buffer;	// Audio buffer that is played by the Speaker
	size_t  buffer_size;	// Block size of the audio buffer
	size_t  buffer_index;	// Block offset of the last written 

	// 2 alternating buffer headers to loop the audio buffer
	WAVEHDR buffer_headerA;
	WAVEHDR buffer_headerB;

	thread update_thread;	// Updates the audio buffer on a timer

	// List of Audio Sources that the stream is playing
	// Each source is mixed together when added to the buffers
	struct AudioNode
	{	AudioSource source;
		AudioNode* next;
	};

	AudioNode* head;
	AudioNode* tail;

	long long last_error;

public:
	AudioOutput();

	// Finds the closest supported wave format of the speaker device 
	void configFormat(size_t deviceID);

	// Opens an audio output device with the closest supported format
	// Sets up the audio buffers and starts the update thread
	// If there was already a device opened, it will close it first
	int openDevice(size_t deviceID);

	// Closes the audio output device if one was open
	int closeDevice();

	// Gets the supported wave formats of the device
	unsigned long long getAvailFmts(size_t deviceID);

	// Sets the wave format of the speaker device
	int setFormat(WaveFmt fmt);
	int setFormat(short channels, short bitsPerSample, long samplesPerSec);

	// Creates and returns a new Audio Source ties to the Audio Output
	AudioSource* createSource(unsigned char flags = 0);

	// Gets n blocks of audio data mixed from all Audio Sources
	void getAudioData(char* buffer, int blocks);

	// Mix n blocks of audio data B into A
	void mixAudioData(char* A, char* B, int buffer_size, int sample_size);

	// Loads n blocks of data from a buffer to the Audio Buffer
	// Loading starts from the last index, and if there isn't enough space,
	// data wraps around at the beginning
	void loadAudioBuffer(char* buffer, int blocks);

	// The audio buffers are deallocated and the update thread is stopped
	void freeResources();
};

int adjustFormat(const unsigned long long supported, const WaveFmt &sample, WaveFmt &adjusted);

#endif