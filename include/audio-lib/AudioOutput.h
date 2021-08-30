#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include "AudioSource.h"

#pragma comment(lib, "Winmm.lib")

class AudioOutput
{
public:
	// Speaker device for playing the audio
	HWAVEOUT speaker;

	// Desired and supported format of the speaker device
	// If the speaker is not capable of supporting the desired format
	// The samples will be converted to the supported format
	WAVEFORMATEX desired_fmt;
	WAVEFORMATEX supported_fmt;

	// Alternating Playing-Prepared audio buffers of 1/100 of a second length
	// When buffer A starts playing, B is prepared and vice versa.
	char*   audio_buffer1;
	WAVEHDR buffer1_header;
	char*   audio_buffer2;
	WAVEHDR buffer2_header;

	// List of Audio Sources that the stream is playing
	// Each source is mixed together when added to the buffers
	struct AudioNode
	{
		AudioSource source;
		AudioNode* next;
	};

	AudioNode* head;
	AudioNode* tail;

public:
	AudioOutput();

	AudioSource* createSource(WAVEFORMATEX fmt, unsigned char flags = 0);

	//void deleteSource();
	//void setFormat();
	//void getFormat();

	void mixAudioSources(char* buffer);

	int openDevice(size_t deviceID);

	void closeDevice();
};

#endif