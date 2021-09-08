#include <audio-lib/AudioSource.h>

AudioSource::AudioSource(WaveFmt fmt, unsigned char flags)
	: audio_fmt(fmt),
	head(NULL), tail(NULL), curr(NULL), offset(0),
	empty_persist( (flags & AS_FLAG_PERSIST ) > 0),
	data_buffered( (flags & AS_FLAG_BUFFERED) > 0),
	audio_looped ( (flags & AS_FLAG_LOOPED  ) > 0)
{}

AudioSource::~AudioSource()
{
	clear();
}

// Adds n blocks of data to the end of the Audio Source
// The input is chopped into smaller units and converted to the
// format of the Audio Source. both the original and the converted data
// is kept, along with the original format of the data
void AudioSource::add(const char* data, size_t blocks, const WaveFmt &fmt)
{
	if(	conv.in_fmt.numChannels != fmt.numChannels ||
		conv.in_fmt.bitsPerSample != fmt.bitsPerSample ||
		conv.in_fmt.sampleRate != fmt.sampleRate)
	{
		// Re-Initialize Converter
	}

	int max_blocks_in = conv.max_input * MAX_NODE_UNITS;	
	int max_blocks_out =  conv.max_output * MAX_NODE_UNITS ;
	DataNode* this_node;

	while(blocks > 0)
	{
		this_node = new DataNode{
			fmt.sampleRate,
			fmt.numChannels,
			fmt.bitsPerSample,
			NULL, 0,
			NULL, 0,
			NULL
		};

		this_node->origin    = new char[max_blocks_in * conv.in_fmt.blockAlign];
		this_node->orig_len  = max_blocks_in;
		this_node->processed = new char[max_blocks_out * conv.out_fmt.blockAlign];

		if(blocks > max_blocks_in)
		{	
			memcpy(this_node->origin, data, max_blocks_in * conv.in_fmt.blockAlign);

			this_node->proc_len = conv.convert(
				this_node->origin,
				this_node->processed,
				max_blocks_in
			);

			blocks -= max_blocks_in;
		}
		else
		{
			memcpy(this_node->origin, data, blocks * conv.in_fmt.blockAlign);

			this_node->proc_len = conv.convert(
				this_node->origin,
				this_node->processed,
				blocks
			);

			blocks -=blocks;
		}

		if (head == NULL)
		{	head = this_node;
			tail = head;
			curr = head;
		}
		else
		{	tail->next = this_node;
			tail = tail->next;
		}
	}
}


// Takes n blocks of data from the Audio Source across Data Nodes
// One block of data depends on the channels, sampling freq. and sample size
// The requested wave format provided in "fmt" (conversion applies)
void AudioSource::take(char* buff, size_t blocks, const WaveFmt &fmt)
{
	size_t input_size, output_size, work_size, copy_amount;
	char *copy_from, *copy_to;
	char *tmp1, *tmp2;

	input_size = audio_fmt.blockAlign * blocks;
	output_size = fmt.blockAlign * blocks;
	work_size = MAX(input_size, output_size);

	tmp1 = new char[work_size];
	copy_to = tmp1;

	for (copy_to = tmp1; blocks > 0; copy_to += copy_amount)
	{
		// Case where there is no more data
		if (curr == NULL)
		{
			copy_amount = blocks * audio_fmt.blockAlign;
			memset(copy_to, 0, copy_amount);
			blocks = 0;
		}
		// Case where this is the last data block needed
		else if (curr->length - offset > blocks)
		{
			copy_from = curr->bytes + offset * audio_fmt.blockAlign;
			copy_amount = blocks * audio_fmt.blockAlign;
			memcpy(copy_to, copy_from, copy_amount);

			offset += blocks;
			blocks = 0;
		}
		// Case where the data block is fully consumed
		else
		{
			copy_from = curr->bytes + offset * audio_fmt.blockAlign;
			copy_amount = (curr->length - offset) * audio_fmt.blockAlign;
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


	if (audio_fmt.numChannels == fmt.numChannels &&
		audio_fmt.sampleRate == fmt.sampleRate &&
		audio_fmt.bitsPerSample == fmt.bitsPerSample)
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