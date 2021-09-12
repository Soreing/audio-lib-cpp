#include <audio-lib/AudioOutput.h>

#include <thread>
#include <chrono>
#include <iostream>

using std::chrono::steady_clock;
using std::chrono::milliseconds;
using std::chrono::duration;
using std::chrono::duration_cast;

#define FIRST_BIT_DISTANCE(num, dist) for(dist=0; (num&1) == 0; num>>=1, dist++);
#define LAST_BIT_DISTANCE(num, dist)  for(dist=0; (num>>=1) != 0; dist++);

unsigned long long channelMasks[]   = { 0xFFFFFFFFFFFFFFFFull, 0xAAAAAAAAAAAAAAAAull, 0x0000000000000000ull };
unsigned long long sampSizeMasks[]  = { 0xFFFFFFFFFFFFFFFFull, 0xCCCCCCCCCCCCCCCCull, 0x0000000000000000ull };
unsigned long long frequencyMasks[] = { 
    0xFFFFFFFFFFFFFFF0ull, 0xFFFFFFFFFFFFFF00ull, 0xFFFFFFFFFFFFF000ull, 0xFFFFFFFFFFFF0000ull,
    0xFFFFFFFFFFF00000ull, 0xFFFFFFFFFF000000ull, 0xFFFFFFFFF0000000ull, 0xFFFFFFFF00000000ull,
    0xFFFFFFF000000000ull, 0xFFFFFF0000000000ull, 0xFFFFF00000000000ull, 0xFFFF000000000000ull,
    0xFFF0000000000000ull, 0xFF00000000000000ull, 0xF000000000000000ull, 0x0000000000000000ull, };

const short ChannelList[2]   = { 1,  2 };
const short SampleList[2]    = { 8, 16 };
const long  FrequencyList[11] = { 
    8000,  11025, 16000, 22050, 
    24000, 32000, 44100, 48000, 
    88200, 96000, 0 };

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
	{	
		aout->state   = Stopped;
		aout->speaker = 0;
		aout->freeResources();
	}
}

// Loads audio data from Audio Sources to the Audio Buffer in set intervals
// The thread goes to sleep between updates and quits when the speaker is closed
void audioLoaderThread(void* lparam)
{
	AudioOutput* aout = (AudioOutput*)lparam;

	long long interval   = 1000 / BUFFER_FRACTION;	// Time interval in milliseconds for the updates
	long long next_count = 0;						// Time in milliseconds for the next update

	int load_per_second  = aout->supported_fmt.sampleRate;
	int load_accumulator = 0;
	int load_amount      = 0;

	// Temporary buffer to store audio data
	size_t buffer_bytes = aout->mixer_size * aout->supported_fmt.blockAlign;
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

// Finds the closest supported wave format of the speaker device
void AudioOutput::configFormat(size_t deviceID)
{
	WAVEFORMATEX fmt;
	memcpy(&fmt, &desired_fmt, sizeof(WaveFmt));
	fmt.cbSize = 0;

	last_error = waveOutOpen(
		&speaker, 
		(UINT)deviceID, 
		&fmt, 
		NULL, 
		NULL, 
		WAVE_FORMAT_QUERY
	);

	if(last_error == 0)
	{	supported_fmt = desired_fmt;
	}
	else
	{	adjustFormat(getAvailFmts(deviceID), desired_fmt, supported_fmt);
	}
}

// Opens an audio output device with the closest supported format
// Sets up the audio buffers and starts the update thread
// If there was already a device opened, it will close it first
int AudioOutput::openDevice(size_t deviceID)
{
	closeDevice();
	configFormat(deviceID);

	WAVEFORMATEX fmt;
	memcpy(&fmt, &supported_fmt, sizeof(WaveFmt));
	fmt.cbSize = 0;

	last_error = waveOutOpen(
		&speaker, 
		(UINT)deviceID, 
		&fmt, 
		(DWORD_PTR)audioCallback, 
		(DWORD_PTR)this, 
		CALLBACK_FUNCTION
	);

	if (last_error == 0)
	{	
		buffer_index = 0;
		buffer_size  = supported_fmt.sampleRate / BUFFER_FRACTION * BUFFER_SEGMENTS;
		mixer_size   = supported_fmt.sampleRate / BUFFER_FRACTION + 1;

		size_t buffer_bytes = buffer_size * supported_fmt.blockAlign;
		size_t mixer_bytes  = mixer_size  * supported_fmt.blockAlign;

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
AudioSource* AudioOutput::createSource(unsigned char flags)
{
	if (head == NULL)
	{	head = new AudioNode{ AudioSource(supported_fmt, flags), NULL };
		tail = head;
	}
	else
	{	//tail->next = new AudioNode{ AudioSource(supported_fmt, flags), NULL };
		tail = tail->next;
	}

	return &tail->source;
}

int adjustFormat(const unsigned long long supported, const WaveFmt &sample, WaveFmt &adjusted)
{
	size_t num_channels_index = sample.numChannels - 1;
	size_t sample_size_index = (sample.bitsPerSample >> 3) - 1;
	size_t frequency_index;

	for(frequency_index=0; FrequencyList[frequency_index] != 0; frequency_index++)
	{	if(sample.sampleRate <= FrequencyList[frequency_index])
		{	break;
		}
	}

	if(	num_channels_index <= 1 && sample_size_index <= 1 && (sample.bitsPerSample & 7) == 0)
	{	
        unsigned long long faster_sampling    = supported &  frequencyMasks[frequency_index];
        unsigned long long slower_sampling    = supported & ~frequencyMasks[frequency_index];
        unsigned long long fast_worse_channel = faster_sampling & sampSizeMasks[sample_size_index];
        unsigned long long slow_worse_channel = slower_sampling & sampSizeMasks[sample_size_index];
        unsigned long long fast_match         = fast_worse_channel & channelMasks[num_channels_index];

        int format_offset = -1;

        if(fast_match !=0) 
            { FIRST_BIT_DISTANCE(fast_match, format_offset); }
        else if(fast_worse_channel != 0) 
            { FIRST_BIT_DISTANCE(fast_worse_channel, format_offset); }
        else if(slow_worse_channel != 0) 
            { LAST_BIT_DISTANCE(slow_worse_channel, format_offset); }
        else if(faster_sampling != 0) 
            { FIRST_BIT_DISTANCE(faster_sampling, format_offset); }
        else if(slower_sampling != 0) 
            { LAST_BIT_DISTANCE(slower_sampling, format_offset); }

        if(format_offset != -1)
        {   int f = format_offset  >> 2;
            int s = (format_offset >> 1) & 1;
            int c = format_offset & 1;

            adjusted = makeWaveFmt(ChannelList[c], SampleList[s], FrequencyList[f]);
            return 0;
        }
    }

	return -1;
}

unsigned long long AudioOutput::getAvailFmts(size_t deviceID)
{
	HWAVEOUT device;
	WAVEFORMATEX winfmt;
	WaveFmt fmt;

	winfmt.cbSize = 0;

	unsigned long long supported   = 0;
	long long format_mask = 1;

	for(int i=0; i<10; i++)
	{
		fmt = makeWaveFmt(_Mono, _8Bit, FrequencyList[i]);
		memcpy(&winfmt, &fmt, sizeof(WaveFmt));

		last_error = waveOutOpen(&device,  (UINT)0, &winfmt, NULL, NULL, WAVE_FORMAT_QUERY);
		if(last_error == 0) { supported |= format_mask; }
		format_mask <<= 1;

		winfmt.nChannels = _Stereo;
		winfmt.nBlockAlign <<= 1;
		winfmt.nAvgBytesPerSec <<= 1;

		last_error = waveOutOpen(&device,  (UINT)0, &winfmt, NULL, NULL, WAVE_FORMAT_QUERY);
		if(last_error == 0) { supported |= format_mask; }
		format_mask <<= 1;

		winfmt.nChannels = _Mono;
		winfmt.wBitsPerSample = _16Bit;

		last_error = waveOutOpen(&device,  (UINT)0, &winfmt, NULL, NULL, WAVE_FORMAT_QUERY);
		if(last_error == 0) { supported |= format_mask; }
		format_mask <<= 1;

		winfmt.nChannels = _Stereo;
		winfmt.nBlockAlign <<= 1;
		winfmt.nAvgBytesPerSec <<= 1;

		last_error = waveOutOpen(&device,  (UINT)0, &winfmt, NULL, NULL, WAVE_FORMAT_QUERY);
		if(last_error == 0) { supported |= format_mask; }
		format_mask <<= 1;
	}

	return supported;
}

int AudioOutput::setFormat(WaveFmt fmt)
{
	desired_fmt = fmt;
	supported_fmt = fmt;

	if(speaker != 0)
	{
		int devID;
		last_error = waveOutGetID( speaker, (LPUINT)&devID);

		if(last_error == 0)
		{
			closeDevice();
			if(last_error == 0)
			{
				std::cout<< "Hello";
				AudioNode* tmp = head;
				while(tmp != NULL)
				{	tmp->source.reset_format(supported_fmt);
					tmp = tmp->next;
				}

				std::cout<< "Bye";

				openDevice(devID);
				if(last_error == 0)
				{	return 0;
				}
			}
		}
	}

	return -1;
}

int AudioOutput::setFormat(short channels, short bitsPerSample, long samplesPerSec)
{
	WaveFmt fmt = makeWaveFmt(
		channels, 
		bitsPerSample, 
		samplesPerSec
	);
	
	return setFormat(fmt);
}

// Gets n blocks of audio data mixed from all audio sources
void AudioOutput::getAudioData(char* buffer, int blocks)
{
	int buffer_bytes = blocks * supported_fmt.blockAlign;
	int sample_size  = supported_fmt.bitsPerSample >> 3;


	AudioNode* tmp = head;
	while (tmp != NULL)
	{	(tmp->source).take(mixer_buffer, blocks);
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
	int align = supported_fmt.blockAlign;
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
	speaker =  0;
	state   = Stopped;

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