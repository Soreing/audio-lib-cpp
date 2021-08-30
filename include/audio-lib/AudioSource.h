#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include <windows.h>

#define AS_FLAG_PERSIST  1
#define AS_FLAG_BUFFERED 2
#define AS_FLAG_LOOPED   4

#define MAX(a, b)  (a > b ? a : b)

class AudioSource
{
	struct DataNode
	{	char* bytes;		// Bytes of data
		size_t length;		// Number of blocks of data
		DataNode* next;		// Pointer to the next Node
	};

	WAVEFORMATEX audio_fmt;	// Format of the Audio Source

	DataNode* head;			// Start of the Audio Data
	DataNode* tail;			// End of the Audio Data
	DataNode* curr;			// Pointer to the current block of data
	size_t    offset;		// Block Offset in the current block of Data

	bool empty_persist : 1;	// Audio Source should not be deleted if it reached the end
	bool data_buffered : 1;	// Data is left in the buffer after taken (can be rewinded)
	bool audio_looped : 1;	// Audio source is looped to play indefinitely

public:

	AudioSource(WAVEFORMATEX fmt, unsigned char flags = 0);

	~AudioSource();

	// Adds n blocks of data to the end of the Audio Source
	// One block of data depends on the channels, sampling freq. and sample size (nBlockAlign)
	// The data being added has the wave format "fmt" (converted to the fmt of the Audio Source)
	void add(const char* data, size_t blocks, const WAVEFORMATEX &fmt);

	// Takes n blocks of data from the Audio Source across Data Nodes
	// One block of data depends on the channels, sampling freq. and sample size
	// The requested wave format provided in "fmt" (conversion applies)
	void take(char* buff, size_t blocks, const WAVEFORMATEX &fmt);

	// Clears the data from the Audio Source
	// Deallocates all resources and resets pointers
	void clear();

	// Rewinds the data to the beginning
	// Only has an effect if the data is buffered
	void rewind();
};

#endif AUDIOSOURCE_H
