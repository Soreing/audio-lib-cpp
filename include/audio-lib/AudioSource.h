#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include <audio-lib/wave.h>
#include <audio-lib/conversion.h>
#include <cpthread/cpevent.h>
#include <cpthread/cpmutex.h>
#include <cpthread/cpthread.h>
#include <windows.h>
#include <iostream>

#define AS_FLAG_PERSIST  1
#define AS_FLAG_BUFFERED 2
#define AS_FLAG_LOOPED   4

#define MAX_NODE_UNITS 1

#define MAX(a, b)  (a > b ? a : b)

class AudioSource
{
	struct DataNode
	{	WaveFmt fmt;			// Wave format of the original data

		char* origin;			// Bytes of the original data
		size_t orig_len;		// Number of blocks in the original data

		char* processed;		// Bytes of converted data ready for use
		size_t proc_len;		// Number of blocks in the processed data

		DataNode* next;			// Pointer to the next Node
	};

	FormatConverter converter;  // Primary data format converter for adding new input
	WaveFmt audio_fmt;			// Format of the Audio Source

	thread post_handler;		// Handles data processing after the starting node
	thread pre_handler;			// Handles data processing from the beginning to the starting node
	bool   handler_active;		// State of the data handler threads 

	signal insert_sig;			// Event signal to notify the handler of new data added
	mutex  proc_mutex;			// Mutex for modifying the processed node pointer
	DataNode* proc;				// Pointer to the current block of data being processed

	DataNode* head;				// Start of the Audio Data
	DataNode* tail;				// End of the Audio Data
	DataNode* curr;				// Pointer to the current block of data being played
	size_t    offset;			// Block Offset in the current block of Data

	bool empty_persist : 1;		// Audio Source should not be deleted if it reached the end
	bool data_buffered : 1;		// Data is left in the buffer after taken (can be rewinded)
	bool audio_looped : 1;		// Audio source is looped to play indefinitely

public:

	AudioSource(WaveFmt fmt, unsigned char flags = 0);

	~AudioSource();

	// Adds n blocks of data to the end of the Audio Source
	// The input is chopped into smaller units and converted to the
	// format of the Audio Source. both the original and the converted data
	// is kept, along with the original format of the data
	void add(const char* data, size_t blocks, const WaveFmt &fmt);

	// Adds n blocks of data to the end of the Audio Source
	// The input is chopped into smaller units but doesn't get
	void add_async(const char* data, size_t blocks, const WaveFmt &fmt);

	// Takes n blocks of data from the Audio Source across Data Nodes
	// If the Source ran out of data, 0s are returned. If the format of the
	// next node is different, the Source is paused and 0s are returned
	void take(char* buff, size_t blocks);

	// Resets the format and hte filter of the audio source and 
	// clears all processed data that was converted from the original samples
	void reset_format(const WaveFmt &fmt);

	// Processes a single node's original data with the current Format Converter
	void process_node(FormatConverter *cnv, DataNode *node);

	// Clears the data from the Audio Source
	// Deallocates all resources and resets pointers
	void clear();

	// Removes the nodes of of audio data up till the current current 
	// if the source is not buffered. If empty, tail is set to NULL
	void remove();

	// Rewinds the data to the beginning
	// Only has an effect if the data is buffered
	void rewind();

	friend THREAD primary_data_processor(void* lparam);
	friend THREAD secondary_data_processor(void* lparam);
};

#endif AUDIOSOURCE_H
