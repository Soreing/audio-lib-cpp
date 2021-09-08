#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include <audio-lib/wave.h>
#include <audio-lib/conversion.h>
#include <windows.h>

#define AS_FLAG_PERSIST  1
#define AS_FLAG_BUFFERED 2
#define AS_FLAG_LOOPED   4

#define MAX_NODE_UNITS 10

#define MAX(a, b)  (a > b ? a : b)

class AudioSource
{
	struct DataNode
	{	long  sample_rate;		// Sample Rate of the original data
		short num_channels;		// Number of channels in the original data
		short bit_depth;		// Bit Depth of the original data

		char* origin;			// Bytes of the original data
		size_t orig_len;		// Number of blocks in the original data

		char* processed;		// Bytes of converted data ready for use
		size_t proc_len;		// Number of blocks in the processed data

		DataNode* next;			// Pointer to the next Node
	};

	FormatConverter conv;		// Format converter to convert input data
	WaveFmt audio_fmt;			// Format of the Audio Source

	DataNode* head;				// Start of the Audio Data
	DataNode* tail;				// End of the Audio Data
	DataNode* curr;				// Pointer to the current block of data
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

	// Takes n blocks of data from the Audio Source across Data Nodes
	// If the Source ran out of data, 0s are returned. If the format of the
	// next node is different, the Source is paused and 0s are returned
	void take(char* buff, size_t blocks);

	// Clears the data from the Audio Source
	// Deallocates all resources and resets pointers
	void clear();

	// Removes the nodes of of audio data up till the current current 
	// if the source is not buffered. If empty, tail is set to NULL
	void remove();

	// Rewinds the data to the beginning
	// Only has an effect if the data is buffered
	void rewind();
};

#endif AUDIOSOURCE_H
