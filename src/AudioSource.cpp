#include <audio-lib/AudioSource.h>
#include <iostream>

THREAD audio_data_manager(void* lparam)
{
	AudioSource* asrc = (AudioSource*)lparam;
	AudioSource::DataNode* this_node;

	while(true)
	{
		this_node = asrc->proc;

		if(this_node != NULL)
		{
			// Reset the Format Converter if the new node's format is different 
			if(	asrc->conv.in_fmt != this_node->fmt)
			{	asrc->conv.init(this_node->fmt, asrc->audio_fmt);
			}

			// If the node is unprocessed, process it
			if(this_node->processed == NULL)
			{	asrc->process_node(this_node);
			}

			// If the next node is not NULL, move to it
			if(asrc->proc->next != NULL)
			{	asrc->proc = asrc->proc->next;
			}
			// If the pointer reached the end, check if the beginning is processed
			// If not, start processing from the beginning
			else if(asrc->head->processed == NULL)
			{	asrc->proc = asrc->head;
			}
			// If all nodes are processed, go to sleep
			else
			{	asrc->handler_sig.wait();
			}
		}

	}
}

AudioSource::AudioSource(WaveFmt fmt, unsigned char flags)
	: audio_fmt(fmt),
	head(NULL), tail(NULL), curr(NULL), proc(NULL), offset(0),
	empty_persist( (flags & AS_FLAG_PERSIST ) > 0),
	data_buffered( (flags & AS_FLAG_BUFFERED) > 0),
	audio_looped ( (flags & AS_FLAG_LOOPED  ) > 0),
	conv(fmt, fmt)
{
	data_handler.create(audio_data_manager, this);
}

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
	int copy_amount;

	char* src = (char*)data;
	DataNode* this_node;

	while(blocks > 0)
	{
		this_node = new DataNode{ fmt, NULL, 0, NULL, 0, NULL };

		copy_amount = blocks > max_blocks_in ? max_blocks_in : blocks;

		this_node->origin    = new char[max_blocks_in  * fmt.blockAlign];
		this_node->orig_len  = copy_amount;

		memcpy(this_node->origin, src, copy_amount * fmt.blockAlign);
		process_node(this_node);

		src += copy_amount * audio_fmt.blockAlign;
		blocks -= copy_amount;

		if (head == NULL)
		{	head = this_node;
			tail = head;
			curr = head;
			proc = head;
		}
		else
		{	tail->next = this_node;
			tail = tail->next;
		}
	}
}

// Adds n blocks of data to the end of the Audio Source
// The input is chopped into smaller units but doesn't get
void AudioSource::add_async(const char* data, size_t blocks, const WaveFmt &fmt)
{
	size_t max_blocks_in = FormatConverter::find_max_input_size(fmt) * MAX_NODE_UNITS;	
	int copy_amount;
	
	char* src = (char*)data;
	DataNode* this_node;

	while(blocks > 0)
	{
		this_node   = new DataNode{ fmt, NULL, 0, NULL, 0, NULL };
		copy_amount = blocks > max_blocks_in ? max_blocks_in : blocks;

		this_node->origin   = new char[max_blocks_in * fmt.blockAlign];
		this_node->orig_len = copy_amount;

		memcpy(this_node->origin, src, copy_amount * fmt.blockAlign);

		src += copy_amount * fmt.blockAlign;
		blocks -= copy_amount;

		if (head == NULL)
		{	head = this_node;
			tail = head;
			curr = head;
			proc = head;
		}
		else
		{	tail->next = this_node;
			tail = tail->next;
		}
	}

	handler_sig.set();
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
		// Case where there is no more data or data is unconverted
		if (curr == NULL || curr->processed == NULL)
		{
			copy_amount = blocks * audio_fmt.blockAlign;
			memset(copy_to, 0, copy_amount);
			blocks = 0;
		}
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
	data_handler.terminate();
	
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

// Resets the format and hte filter of the audio source and 
// clears all processed data that was converted from the original samples
void AudioSource::reset_format(const WaveFmt &fmt)
{
	audio_fmt = fmt;
	conv.init(fmt, fmt);

	DataNode* tmp = head;
	DataNode* nxt = NULL;

	while (tmp != NULL)
	{
		nxt = tmp->next;
		tmp->proc_len = 0;
		delete[] tmp->processed;
		tmp = nxt;
	}

	proc   = curr;
	offset = 0;

	data_handler.create(audio_data_manager, this);
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