#include <audio-lib/AudioOutput.h>

#include <thread>
#include <chrono>
#include <iostream>

using std::chrono::steady_clock;
using std::chrono::milliseconds;
using std::chrono::duration;
using std::chrono::duration_cast;

void CALLBACK audioCallback(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	AudioOutput* aout = (AudioOutput*)dwInstance;

	// If the device finished playing a section of the audio buffer,
	// put it back for playing the region again
	if (uMsg == WOM_DONE)
	{	if(aout->state == Playing)
		{	waveOutWrite(aout->speaker, (WAVEHDR*)dwParam1, sizeof(WAVEHDR));
		}
	}

	// If the audio device was closed, clean up the resources
	if (uMsg == WOM_CLOSE)
	{	aout->freeResources();
	}
}

// Loads audio data from Audio Sources to the Audio Buffer in set intervals
// The thread goes to sleep between updates and quits when the speaker is closed
void audioLoaderThread(void* lparam)
{
	AudioOutput* aout = (AudioOutput*)lparam;

	long long interval   = 1000 / BUFFER_FRACTION;	// Time interval in milliseconds for the updates
	long long next_count = 0;						// Time in milliseconds for the next update

	int load_per_second  = aout->supported_fmt.nSamplesPerSec;
	int load_accumulator = 0;
	int load_amount      = 0;

	// Temporary buffer to store audio data
	size_t buffer_bytes = aout->mixer_size * aout->supported_fmt.nBlockAlign;
	char*  temp_buffer  = new char[buffer_bytes];

	// Timer variables for putting the thread to sleep
	steady_clock::time_point start, now;
	duration<long long, std::milli> time_elapsed;
	start = steady_clock::now();

	// Run while the speaker object is open
	while (aout->speaker != 0)
	{	
		// Time in milliseconds for the next update
		next_count += interval;

		// Find the amount of data to be loaded 
		load_accumulator += load_per_second;
		load_amount = load_accumulator / BUFFER_FRACTION;
		load_accumulator -= load_amount * BUFFER_FRACTION;

		// Get and load the audio data to the Audio Buffer
		memset(temp_buffer, 0, buffer_bytes);
		aout->getAudioData(temp_buffer, load_amount);
		aout->loadAudioBuffer(temp_buffer, load_amount);

		// SLEEP TILL NEXT UPDATE
		now = steady_clock::now();
		time_elapsed = duration_cast<milliseconds>(now - start);
		std::this_thread::sleep_for(milliseconds(next_count - time_elapsed.count()));
	}

	delete[] temp_buffer;
}

AudioOutput::AudioOutput()
	: 	speaker(NULL), state(Stopped),
		audio_buffer(NULL), mixer_buffer(NULL),
		head(NULL), tail(NULL)
{
}

// Opens an audio output device with the closest supported format
// Sets up the audio buffers and starts the update thread
// If there was already a device opened, it will close it first
int AudioOutput::openDevice(size_t deviceID)
{
	closeDevice();

	last_error = waveOutOpen(
		&speaker, 
		(UINT)0, 
		&supported_fmt, 
		(DWORD_PTR)audioCallback, 
		(DWORD_PTR)this, 
		CALLBACK_FUNCTION
	);

	if (last_error == 0)
	{
		buffer_index = 0;
		buffer_size  = supported_fmt.nSamplesPerSec / BUFFER_FRACTION * BUFFER_SEGMENTS;
		mixer_size   = supported_fmt.nSamplesPerSec / BUFFER_FRACTION + 1;

		size_t buffer_bytes = buffer_size * supported_fmt.nBlockAlign;
		size_t mixer_bytes  = mixer_size  * supported_fmt.nBlockAlign;

		audio_buffer = new char[buffer_bytes];
		mixer_buffer = new char[mixer_bytes];
		memset(audio_buffer, 0, buffer_bytes);
		memset(mixer_buffer, 0, mixer_bytes);

		memset(&buffer_headerA, 0, sizeof(WAVEHDR));
		buffer_headerA.lpData = audio_buffer;
		buffer_headerA.dwBufferLength = buffer_bytes / 2;

		last_error = waveOutPrepareHeader(speaker, &buffer_headerA, sizeof(WAVEHDR));
		if(last_error == 0)
		{	
			last_error = waveOutWrite(speaker, &buffer_headerA, sizeof(WAVEHDR));
			if (last_error == 0)
			{
				memset(&buffer_headerB, 0, sizeof(WAVEHDR));
				buffer_headerB.lpData = audio_buffer + buffer_bytes / 2;
				buffer_headerB.dwBufferLength = buffer_bytes / 2;

				last_error = waveOutPrepareHeader(speaker, &buffer_headerB, sizeof(WAVEHDR));
				if (last_error == 0)
				{
					last_error = waveOutWrite(speaker, &buffer_headerB, sizeof(WAVEHDR));
					if (last_error == 0)
					{
						update_thread = std::thread(audioLoaderThread, (void*)this);
						state = Playing;
						return 0;
					}
				}
			}
		}
	}
		
	freeResources();
	return -1;
}

// Closes the audio output device if one was open
int AudioOutput::closeDevice()
{
	if (speaker != 0)
	{	
		state = Stopped;
		last_error = waveOutReset(speaker);
		if(last_error == 0)
		{	
			last_error = waveOutClose(speaker);
			if(last_error == 0)
			{	return 0;
			}
		}
	}

	return -1;
}


// Creates and returns a new Audio Source ties to the Audio Output
AudioSource* AudioOutput::createSource(WAVEFORMATEX fmt, unsigned char flags)
{
	if (head == NULL)
	{	head = new AudioNode{ AudioSource(fmt, flags), NULL };
		tail = head;
	}
	else
	{	tail->next = new AudioNode{ AudioSource(fmt, flags), NULL };
		tail = tail->next;
	}

	return &tail->source;
}

// Gets n blocks of audio data mixed from all audio sources
void AudioOutput::getAudioData(char* buffer, int blocks)
{
	int buffer_bytes = blocks * supported_fmt.nBlockAlign;
	int sample_size  = supported_fmt.wBitsPerSample >> 3;


	AudioNode* tmp = head;
	while (tmp != NULL)
	{	(tmp->source).take(mixer_buffer, blocks, supported_fmt);
		mixAudioData(buffer, mixer_buffer, buffer_bytes, sample_size);
		tmp = tmp->next;
	}
}

// Mix n blocks of audio data B into A
void AudioOutput::mixAudioData(char* A, char* B, int buffer_size, int sample_size)
{
	long long llA;
	long long llB;
	long long accumulator;
	
	for(int i = 0; i < buffer_size; i += sample_size)
	{
		if(sample_size == 1)
		{	llA = *(char*)(A+i);
			llB = *(char*)(B+i);
			accumulator = llA + llB - ((llA*llB) >> 8);
			*(char*)(A+i) = (char)accumulator;
		}
		else if(sample_size == 2)
		{	llA = *(short*)(A+i);
			llB = *(short*)(B+i);
			accumulator = llA + llB - ((llA*llB) >> 16);
			*(short*)(A+i) = (short)accumulator;
		}
		else if(sample_size == 4)
		{	llA = *(int*)(A+i);
			llB = *(int*)(B+i);
			accumulator = llA + llB - ((llA*llB) >> 32);
			*(int*)(A+i) = (int)accumulator;
		}
	}
}

// Loads blocks of data from a buffer to the Audio Buffer
// Loading starts from the last index, and if there isn't enough space,
// data wraps around at the beginning
void AudioOutput::loadAudioBuffer(char* buffer, int blocks)
{
	int align = supported_fmt.nBlockAlign;
	int first, second;
	
	if(buffer_index + blocks <= buffer_size)
	{	memcpy(audio_buffer + (buffer_index*align), buffer, blocks*align);
		buffer_index += blocks;
	}
	else
	{	first  = buffer_size - buffer_index;
		second = blocks - first;

		memcpy(audio_buffer + (buffer_index*align), buffer, first*align);
		memcpy(audio_buffer, buffer + (first*align), second*align);
		buffer_index = second;
	}
}


// The audio buffers are deallocated and the update thread is stopped
void AudioOutput::freeResources()
{
	speaker = 0;
	update_thread.join();

	if(mixer_buffer != NULL)
	{	delete[] mixer_buffer;
		mixer_buffer = NULL;
	}

	if(audio_buffer != NULL)
	{	delete[] audio_buffer;
		audio_buffer = NULL;
	}
}