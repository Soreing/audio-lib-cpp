#ifndef SAMPLING_H
#define SAMPLING_H

#include <audio-lib/filter.h>

typedef unsigned char uchar;

class RateConverter
{
public:
	size_t L, M;
	size_t num_channels;
	size_t bit_depth;

	FIRFilter_i64 inter_filter;
	size_t* inter_delay_idxs;
	void**  inter_delay_lines;

	FIRFilter_i64 decim_filter;
	size_t* decim_fractions;
	size_t* decim_delay_idxs;
	void**  decim_delay_lines;

public:
	RateConverter();
	RateConverter(size_t L, size_t M, size_t taps, size_t channels, size_t depth);
	~RateConverter();

	void init(size_t L, size_t M, size_t taps, size_t channels, size_t depth);
	void clear();

	int decimation(const uchar* src, uchar* dst, size_t channel, size_t samples);
	int interpolation(const uchar* src, uchar* dst, size_t channel, size_t samples);
	int non_integral(const uchar* src, uchar* dst, size_t channel, size_t samples);

	int decimation(const short* src, short* dst, size_t channel, size_t samples);
	int interpolation(const short* src, short* dst, size_t channel, size_t samples);
	int non_integral(const short* src, short* dst, size_t channel, size_t samples);
};

#endif

