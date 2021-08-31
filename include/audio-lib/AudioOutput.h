#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <windows.h>
#include <thread>
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
	WAVEFORMATEX desired_fmt;
	WAVEFORMATEX supported_fmt;

	char*   mixer_buffer1;	// Buffer for mixing audio sources
	char*   mixer_buffer2;	// Buffer for mixing audio sources
	size_t  mixer_size;		// Block size of the mixer buffer
	char*   audio_buffer;	// Audio buffer that is played by the Speaker
	size_t  buffer_size;	// Block size of the audio buffer
	size_t  buffer_index;	// Block offset of the last written 

	// 2 alternating buffer headers to loop the audio buffer
	WAVEHDR buffer_headerA;
	WAVEHDR buffer_headerB;

	std::thread update_thread;	// Updates the audio buffer on a timer

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

	// Opens an audio output device with the closest supported format
	// Sets up the audio buffers and starts the update thread
	// If there was already a device opened, it will close it first
	int openDevice(size_t deviceID);

	// Closes the audio output device if one was open
	int closeDevice();

	// Creates and returns a new Audio Source ties to the Audio Output
	AudioSource* createSource(WAVEFORMATEX fmt, unsigned char flags = 0);

	// Gets n blocks of audio data mixed from all Audio Sources
	void getAudioData(char* buffer, int blocks);

	// Mix n blocks of audio data B into A
	//void mixAudioData(char* A, char* B, int blocks);

	// Loads n blocks of data from a buffer to the Audio Buffer
	// Loading starts from the last index, and if there isn't enough space,
	// data wraps around at the beginning
	void loadAudioBuffer(char* buffer, int blocks);


	// The audio buffers are deallocated and the update thread is stopped
	void freeResources();
};

#endif