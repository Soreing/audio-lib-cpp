#include <audio-lib/AudioSource.h>
#include <iostream>

AudioSource::AudioSource(WaveFmt fmt, unsigned char flags)
	: audio_fmt(fmt),
	head(NULL), tail(NULL), curr(NULL), offset(0),
	empty_persist( (flags & AS_FLAG_PERSIST ) > 0),
	data_buffered( (flags & AS_FLAG_BUFFERED) > 0),
	audio_looped ( (flags & AS_FLAG_LOOPED  ) > 0),
	conv(fmt, fmt)
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
	if(	conv.in_fmt != fmt)
	{	conv.init(fmt, audio_fmt);
	}

	size_t max_blocks_in  = conv.max_input * MAX_NODE_UNITS;	
	size_t max_blocks_out = conv.max_output * MAX_NODE_UNITS;
	bool matching_format  = (audio_fmt == fmt);
	
	char* src = (char*)data;
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
		this_node->processed = new char[max_blocks_out * conv.out_fmt.blockAlign];

		if(blocks > max_blocks_in)
		{	
			this_node->orig_len  = max_blocks_in;
			memcpy(this_node->origin, src, max_blocks_in * conv.in_fmt.blockAlign);

			if(matching_format)
			{	this_node->proc_len  = max_blocks_in;
				memcpy(this_node->processed, src, max_blocks_in * conv.in_fmt.blockAlign);
			}
			else
			{	this_node->proc_len = conv.convert(
					this_node->origin,
					this_node->processed,
					max_blocks_in
				);

				this_node->proc_len /= conv.out_fmt.blockAlign;
			}

			src += max_blocks_in * conv.in_fmt.blockAlign;
			blocks -= max_blocks_in;
		}
		else
		{
			this_node->orig_len  = blocks;
			memcpy(this_node->origin, src, blocks * conv.in_fmt.blockAlign);

			if(matching_format)
			{	this_node->proc_len  = blocks;
				memcpy(this_node->processed, src, blocks * conv.in_fmt.blockAlign);
			}
			else
			{	this_node->proc_len = conv.convert(
					this_node->origin,
					this_node->processed,
					blocks
				);

				this_node->proc_len /= conv.out_fmt.blockAlign;
			}

			src += blocks * conv.in_fmt.blockAlign;
			blocks -= blocks;
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
// If the Source ran out of data, 0s are returned. If the format of the
// next node is different, the Source is paused and 0s are returned
void AudioSource::take(char* buff, size_t blocks)
{
	size_t copy_amount;
	char *copy_from, *copy_to;

	for (copy_to = buff; blocks > 0; copy_to += copy_amount)
	{
		// Case where there is no more data
		if (curr == NULL)
		{
			copy_amount = blocks * audio_fmt.blockAlign;
			memset(copy_to, 0, copy_amount);
			blocks = 0;
		}
		// Case where the next data's format hasn't been converted yet
		//else if (
		//	audio_fmt.numChannels   !=  curr->num_channels ||
		//	audio_fmt.bitsPerSample !=  curr->bit_depth    ||
		//	audio_fmt.sampleRate    !=  curr->sample_rate )
		//{
		//	copy_amount = blocks * audio_fmt.blockAlign;
		//	memset(copy_to, 0, copy_amount);
		//	blocks = 0;
		//}
		// Case where this is the last data block needed
		else if (curr->proc_len - offset > blocks)
		{
			copy_from = curr->processed + offset * audio_fmt.blockAlign;
			copy_amount = blocks * audio_fmt.blockAlign;
			memcpy(copy_to, copy_from, copy_amount);

			offset += blocks;
			blocks = 0;
		}
		// Case where the data block is fully consumed
		else
		{
			copy_from = curr->processed + offset * audio_fmt.blockAlign;
			copy_amount = (curr->proc_len - offset) * audio_fmt.blockAlign;
			memcpy(copy_to, copy_from, copy_amount);

			blocks -= (curr->proc_len - offset);
			curr = curr->next;
			offset = 0;

			// Delete the block if the Audio Source is not buffered
			if (!data_buffered)
			{	remove();
			}

			// Rewind Audio Source
			if (curr == NULL && audio_looped)
			{	rewind();
			}
		}
	}
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
		delete[] tmp->origin;
		delete[] tmp->processed;
		delete tmp;
		tmp = nxt;
	}

	head = NULL;
	tail = NULL;
	curr = NULL;
	offset = 0;
}

// Removes the nodes of of audio data up till the current current 
// if the source is not buffered. If empty, tail is set to NULL
void AudioSource::remove()
{
	DataNode* tmp = head;
	DataNode* nxt = NULL;

	while (tmp != curr)
	{
		nxt = tmp->next;
		delete[] tmp->origin;
		delete[] tmp->processed;
		delete tmp;
		tmp = nxt;
	}

	head = curr;
	if (head == NULL)
	{	tail = NULL;
	}
}

// Rewinds the data to the beginning
// Only has an effect if the data is buffered
void AudioSource::rewind()
{
	curr = head;
	offset = 0;
}