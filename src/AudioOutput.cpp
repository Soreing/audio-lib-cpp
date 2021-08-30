#include <audio-lib/AudioOutput.h>

void CALLBACK audioLoader(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg == WOM_DONE)
	{
		AudioOutput* stream = (AudioOutput*)dwInstance;
		WAVEHDR* curr = (WAVEHDR*)dwParam1;
		WAVEHDR* next = (WAVEHDR*)curr->dwUser;

		char* buffer = curr->lpData;
		size_t bytes = curr->dwBufferLength;

		memset(buffer, 0, bytes);
		stream->mixAudioSources(buffer);

		waveOutWrite(stream->speaker, curr, sizeof(WAVEHDR));
	}
}

AudioOutput::AudioOutput()
	: speaker(NULL),
	audio_buffer1(NULL), audio_buffer2(NULL),
	head(NULL), tail(NULL)
{
}

AudioSource* AudioOutput::createSource(WAVEFORMATEX fmt, unsigned char flags)
{
	if (head == NULL)
	{
		head = new AudioNode{ AudioSource(fmt, flags), NULL };
		tail = head;
	}
	else
	{
		tail->next = new AudioNode{ AudioSource(fmt, flags), NULL };
		tail = tail->next;
	}

	return &tail->source;
}

//void deleteSource();
//void setFormat();
//void getFormat();

void AudioOutput::mixAudioSources(char* buffer)
{
	AudioNode* tmp = head;
	size_t blocks = supported_fmt.nSamplesPerSec / 20;

	while (tmp != NULL)
	{
		(tmp->source).take(buffer, blocks, supported_fmt);
		tmp = tmp->next;
	}

}

int AudioOutput::openDevice(size_t deviceID)
{
	closeDevice();

	if (waveOutOpen(&speaker, (UINT)0, &supported_fmt, (DWORD_PTR)audioLoader, (DWORD_PTR)this, CALLBACK_FUNCTION) == 0)
	{
		memset(&buffer1_header, 0, sizeof(WAVEHDR));
		memset(&buffer2_header, 0, sizeof(WAVEHDR));

		size_t buffer_size = supported_fmt.nBlockAlign * supported_fmt.nSamplesPerSec / 20;

		audio_buffer1 = new char[buffer_size];
		audio_buffer2 = new char[buffer_size];
		memset(audio_buffer1, 0, buffer_size);
		memset(audio_buffer2, 0, buffer_size);

		buffer1_header.lpData = audio_buffer1;
		buffer1_header.dwBufferLength = buffer_size;
		buffer1_header.dwUser = (DWORD_PTR)&buffer2_header;

		buffer2_header.lpData = audio_buffer2;
		buffer2_header.dwBufferLength = buffer_size;
		buffer2_header.dwUser = (DWORD_PTR)&buffer1_header;

		if (waveOutPrepareHeader(speaker, &buffer1_header, sizeof(WAVEHDR)) == 0)
		{
			if (waveOutWrite(speaker, &buffer1_header, sizeof(WAVEHDR)) == 0) {}
			else { return 3; }
		}
		else { return 2; }

		if (waveOutPrepareHeader(speaker, &buffer2_header, sizeof(WAVEHDR)) == 0)
		{
			if (waveOutWrite(speaker, &buffer2_header, sizeof(WAVEHDR)) == 0) {}
			else { return 5; }
		}
		else { return 4; }

	}
	else { return 1; }
}

void AudioOutput::closeDevice()
{
	if (speaker != 0)
	{
		waveOutClose(speaker);

		delete[] audio_buffer1;
		delete[] audio_buffer2;
		speaker = 0;
	}
}