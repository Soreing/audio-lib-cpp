#include <audio-lib/AudioSource.h>
#include <iostream>

THREAD primary_data_processor(void* lparam)
{
	AudioSource* asrc = (AudioSource*)lparam;
	AudioSource::DataNode* this_node;

	std::cout<< "Primary Started...\n";

	while(asrc->handler_active)
	{
		asrc->proc_mutex.lock();
		this_node = asrc->proc;

		if(this_node != NULL)
		{	
			// Reset the Format Converter if the new node's format is different 
			if(	asrc->converter.in_fmt != this_node->fmt)
			{	asrc->converter.init(this_node->fmt, asrc->audio_fmt);
			}

			asrc->process_node(&(asrc->converter), this_node);

			// Move to the next node for processing it
			asrc->proc = asrc->proc->next;
			asrc->proc_mutex.unlock();
		}
		// If all nodes are processed, go to sleep
		else
		{	std::cout<< "Primary waiting for data...\n";
			asrc->proc_mutex.unlock();
			asrc->insert_sig.wait();
		}
	}

	std::cout<< "Primary Quit...\n";
	return 0;
}

THREAD secondary_data_processor(void* lparam)
{
	AudioSource* asrc = (AudioSource*)lparam;
	AudioSource::DataNode* this_node = asrc->head;
	AudioSource::DataNode* end_node  = asrc->curr;

	std::cout<< "Secondary Started...\n";

	// If the Audio Source starts from the beginning, there is no need
	// for a secondary data processor, and the thread can quit
	if(this_node == NULL)
	{	std::cout<< "Secondary Useless...\n";
		return 0;
	}

	// Set up the converter for the secondary processor
	FormatConverter converter(this_node->fmt, asrc->audio_fmt);

	while(asrc->handler_active)
	{
		// If the secondary processor caught up to the start of the stream
		// The thread can quit as it processed all nodes it is responsible for
		if(this_node == end_node)
		{	break;
		}

		// Reset the Format Converter if the new node's format is different 
		if(	converter.in_fmt != this_node->fmt)
		{	converter.init(this_node->fmt, asrc->audio_fmt);
		}

		asrc->process_node(&converter, this_node);

		// Move to the next node for processing
		this_node = this_node->next;
	}

	std::cout<< "Secondary Quit...\n";
	return 0;
}

/*THREAD audio_data_manager(void* lparam)
{
	AudioSource* asrc = (AudioSource*)lparam;
	AudioSource::DataNode* this_node;

	while(true)
	{
		asrc->proc_mutex.lock();
		this_node = asrc->proc;
		std::cout<< "r";
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

			// Move to the next node for processing it
			asrc->proc = asrc->proc->next;
			asrc->proc_mutex.unlock();
		}
		// If all nodes are processed, go to sleep
		else
		{	asrc->proc_mutex.unlock();
			asrc->handler_sig.wait();
		}

	}
}*/

AudioSource::AudioSource(WaveFmt fmt, unsigned char flags)
	: audio_fmt(fmt),
	head(NULL), tail(NULL), curr(NULL), proc(NULL), offset(0),
	empty_persist( (flags & AS_FLAG_PERSIST ) > 0),
	data_buffered( (flags & AS_FLAG_BUFFERED) > 0),
	audio_looped ( (flags & AS_FLAG_LOOPED  ) > 0),
	converter(fmt, fmt)
{
	handler_active = true;
	pre_handler.create(secondary_data_processor, this);
	post_handler.create(primary_data_processor, this);
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
	/*if(	conv.in_fmt != fmt)
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

		this_node->origin    = new char[copy_amount  * fmt.blockAlign];
		this_node->orig_len  = copy_amount;

		memcpy(this_node->origin, src, copy_amount * fmt.blockAlign);
		process_node(this_node);

		src += copy_amount * audio_fmt.blockAlign;
		blocks -= copy_amount;

		if (head == NULL)
		{	head = this_node;
			tail = head;
			curr = head;
		}
		else
		{	tail->next = this_node;
			tail = tail->next;
		}
	}*/
}

// Adds n blocks of data to the end of the Audio Source
// The input is chopped into smaller units but doesn't get
void AudioSource::add_async(const char* data, size_t blocks, const WaveFmt &fmt)
{
	size_t max_blocks_in = FormatConverter::find_max_input_size(fmt) * MAX_NODE_UNITS;	
	size_t copy_amount;
	
	// Local data and chain pointers
	char* src = (char*)data;
	DataNode* head_node = NULL;
	DataNode* tail_node = NULL;
	DataNode* this_node;

	int size=0;

	// Break input into a local chain of smaller nodes
	while(blocks > 0)
	{
		this_node   = new DataNode{ fmt, NULL, 0, NULL, 0, NULL };
		copy_amount = blocks > max_blocks_in ? max_blocks_in : blocks;

		this_node->origin   = new char[copy_amount * fmt.blockAlign];
		this_node->orig_len = copy_amount;

		memcpy(this_node->origin, src, copy_amount * fmt.blockAlign);
		
		src += copy_amount * fmt.blockAlign;
		size+= sizeof(DataNode);
		blocks -= copy_amount;

		if (head_node == NULL)
		{	head_node = this_node;
			tail_node = this_node;
		}
		else
		{	tail_node->next = this_node;
			tail_node = tail_node->next;
		}
	}

	// Wait for the main chain to be modifiable, then add the local chain to 
	// the main chain. If needed, notify the processor thread to wake up
	proc_mutex.lock();
	
	if(head == NULL)
	{	head = head_node;
		tail = tail_node;
		curr =  head;
	}
	else
	{	tail->next = head_node;
		tail = tail_node;
	}

	if(proc == NULL)
	{	proc = head_node;
		insert_sig.set();
	}

	proc_mutex.unlock();
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
		// Case where there is no more data or the data is unconverted
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
	handler_active = false;
	insert_sig.set();
	pre_handler.join();
	post_handler.join();
	
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
	handler_active = false;
	insert_sig.set();
	pre_handler.join();
	post_handler.join();

	audio_fmt = fmt;
	converter.init(fmt, fmt);

	DataNode* tmp = head;
	DataNode* nxt = NULL;

	while (tmp != NULL)
	{
		nxt = tmp->next;
		delete[] tmp->processed;
		tmp->processed = NULL;
		tmp->proc_len  = 0;
		tmp = nxt;
	}

	proc   = curr;
	offset = 0;

	handler_active = true;
	pre_handler.create(secondary_data_processor, this);
	post_handler.create(primary_data_processor, this);
}

// Processes a single node's original data with the current Format Converter
void AudioSource::process_node(FormatConverter *cnv, DataNode *node)
{
	size_t max_blocks_out = cnv->max_output * MAX_NODE_UNITS;
	char* buffer = new char[max_blocks_out * audio_fmt.blockAlign];

	if(audio_fmt == node->fmt)
	{	node->proc_len = node->orig_len;
		memcpy(	buffer, node->origin, node->orig_len * audio_fmt.blockAlign);
	}
	else
	{	node->proc_len = cnv->convert( node->origin, buffer, node->orig_len);
		node->proc_len /= audio_fmt.blockAlign;
	}

	node->processed = buffer;
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