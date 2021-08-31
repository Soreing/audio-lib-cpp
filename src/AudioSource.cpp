#include <audio-lib/AudioSource.h>

AudioSource::AudioSource(WAVEFORMATEX fmt, unsigned char flags)
	: audio_fmt(fmt),
	head(NULL), tail(NULL), curr(NULL), offset(0),
	empty_persist(flags & AS_FLAG_PERSIST),
	data_buffered(flags & AS_FLAG_BUFFERED),
	audio_looped(flags & AS_FLAG_LOOPED)
{}

AudioSource::~AudioSource()
{
	clear();
}

// Adds n blocks of data to the end of the Audio Source
// One block of data depends on the channels, sampling freq. and sample size (nBlockAlign)
// The data being added has the wave format "fmt" (converted to the fmt of the Audio Source)
void AudioSource::add(const char* data, size_t blocks, const WAVEFORMATEX &fmt)
{
	size_t input_size, output_size, work_size;
	char *tmp1, *tmp2;

	input_size = fmt.nBlockAlign * blocks;
	output_size = audio_fmt.nBlockAlign * blocks;
	work_size = MAX(input_size, output_size);

	if (head == NULL)
	{
		head = new DataNode{ new char[output_size], blocks , NULL };
		tail = head;
		curr = head;
	}
	else
	{
		tail->next = new DataNode{ new char[output_size], blocks , NULL };
		tail = tail->next;
	}

	if (audio_fmt.nChannels == fmt.nChannels &&
		audio_fmt.nSamplesPerSec == fmt.nSamplesPerSec &&
		audio_fmt.wBitsPerSample == fmt.wBitsPerSample)
	{
		memcpy(tail->bytes, data, output_size);
	}
	else
	{
		//tmp1 = new char[work_size];
		//tmp2 = new char[work_size];
	}
}


// Takes n blocks of data from the Audio Source across Data Nodes
// One block of data depends on the channels, sampling freq. and sample size
// The requested wave format provided in "fmt" (conversion applies)
void AudioSource::take(char* buff, size_t blocks, const WAVEFORMATEX &fmt)
{
	size_t input_size, output_size, work_size, copy_amount;
	char *copy_from, *copy_to;
	char *tmp1, *tmp2;

	input_size = audio_fmt.nBlockAlign * blocks;
	output_size = fmt.nBlockAlign * blocks;
	work_size = MAX(input_size, output_size);

	tmp1 = new char[work_size];
	copy_to = tmp1;

	for (copy_to = tmp1; blocks > 0; copy_to += copy_amount)
	{
		// Case where there is no more data
		if (curr == NULL)
		{
			copy_amount = blocks * audio_fmt.nBlockAlign;
			memset(copy_to, 0, copy_amount);
			blocks = 0;
		}
		// Case where this is the last data block needed
		else if (curr->length - offset > blocks)
		{
			copy_from = curr->bytes + offset * audio_fmt.nBlockAlign;
			copy_amount = blocks * audio_fmt.nBlockAlign;
			memcpy(copy_to, copy_from, copy_amount);

			offset += blocks;
			blocks = 0;
		}
		// Case where the data block is fully consumed
		else
		{
			copy_from = curr->bytes + offset * audio_fmt.nBlockAlign;
			copy_amount = (curr->length - offset) * audio_fmt.nBlockAlign;
			memcpy(copy_to, copy_from, copy_amount);

			blocks -= (curr->length - offset);
			curr = curr->next;
			offset = 0;

			// Delete the block if the Audio Source is not buffered
			if (!data_buffered)
			{
				DataNode* tmp = head;
				DataNode* nxt = NULL;

				while (tmp != curr)
				{
					nxt = tmp->next;
					delete[] tmp->bytes;
					delete tmp;
					tmp = nxt;
				}

				head = curr;
				if (head == NULL)
				{
					tail = NULL;
				}
			}

			// Rewind Ausio Source
			if (curr == NULL && audio_looped)
			{
				curr = head;
			}
		}
	}


	if (audio_fmt.nChannels == fmt.nChannels &&
		audio_fmt.nSamplesPerSec == fmt.nSamplesPerSec &&
		audio_fmt.wBitsPerSample == fmt.wBitsPerSample)
	{
		memcpy(buff, tmp1, output_size);
	}
	else
	{
		//tmp1 = new char[work_size];
		//tmp2 = new char[work_size];
	}

	delete[] tmp1;
}

// Clears the data from the Audio Source
// Deallocates all resources and resets pointers
void AudioSource::clear()
{
	DataNode* tmp = head;
	DataNode* nxt = NULL;

	while (tmp != NULL)
	{
		nxt = tmp->next;
		delete[] tmp->bytes;
		delete tmp;
		tmp = nxt;
	}

	head = NULL;
	tail = NULL;
	curr = NULL;
	offset = 0;
}

// Rewinds the data to the beginning
// Only has an effect if the data is buffered
void AudioSource::rewind()
{
	curr = head;
	offset = 0;
}