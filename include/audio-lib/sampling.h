#ifndef SAMPLING_H
#define SAMPLING_H

#include <audio-lib/filter.h>

typedef unsigned char uchar;

class RateConverter
{
public:
	size_t L, M;					// Interpo(L)ation and Deci(M)ation factors of the conversion
	size_t num_channels;			// Number of channels of the data beign converted
	size_t bit_depth;				// Bit Depth of the data being ocnverted

	FIRFilter_i64 inter_filter;		// FIR Filter used for interpolation
	size_t* inter_delay_idxs;		// Indexes of the last data for each channel's interpolation delay line
	void**  inter_delay_lines;		// Delay lines for each channel to store samples for interpolation

	FIRFilter_i64 decim_filter;		// FIR Filter used for decimation
	size_t* decim_fractions;		// Fraction count of the samples present for decimation
	size_t* decim_delay_idxs;		// Indexes of the last data for each channel's decimation delay line
	void**  decim_delay_lines;		// Delay lines for each channel to store samples for decimation

public:
	RateConverter();
	RateConverter(size_t L, size_t M, size_t taps, size_t channels, size_t depth);
	~RateConverter();

	// Initializes the filters and buffers for the converter
	void init(size_t L, size_t M, size_t taps, size_t channels, size_t depth);
	// Deallocates all dynamic resources
	void clear();

	// Decimation of n 8-Bit "samples" from src to dst in a wave with some "channels"
	int decimation(const uchar* src, uchar* dst, size_t channel, size_t samples);
	// Interpolation of n 8-Bit "samples" from src to dst in a wave with some "channels"
	int interpolation(const uchar* src, uchar* dst, size_t channel, size_t samples);
	// Interpolation and Decimation of n 8-Bit "samples" from src to dst in a wave with some "channels"
	int non_integral(const uchar* src, uchar* dst, size_t channel, size_t samples);

	// Decimation of n 16-Bit "samples" from src to dst in a wave with some "channels"
	int decimation(const short* src, short* dst, size_t channel, size_t samples);
	// Interpolation of n 16-Bit "samples" from src to dst in a wave with some "channels"
	int interpolation(const short* src, short* dst, size_t channel, size_t samples);
	// Interpolation and Decimation of n 16-Bit "samples" from src to dst in a wave with some "channels"
	int non_integral(const short* src, short* dst, size_t channel, size_t samples);
};

#endif

