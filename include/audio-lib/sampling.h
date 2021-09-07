#ifndef SAMPLING_H
#define SAMPLING_H

#include "filter.h"

class Resampler
{
	const size_t L, M;
	const size_t num_channels;
	const size_t bit_depth;

	FIRFilter_i64 inter_filter;
	size_t inter_delay_idx;
	void** inter_delay_lines;

	FIRFilter_i64 decim_filter;
	size_t decim_delay_idx;
	void** decim_delay_lines;

	size_t decim_fraction;

public:
	Resampler(size_t L, size_t M, size_t taps, size_t channels, size_t depth);
	~Resampler();

	void decimation_16(const short* src, short* dst, size_t channel, size_t samples);
	void interpolation_16(const short* src, short* dst, size_t channel, size_t samples);
	void non_integral_16(const short* src, short* dst, size_t channel, size_t samples);
};

struct factor
{	int value;
	int count;
};

struct scale
{	int L;
	int M;
};

int get_prime_factors(size_t value, factor* factors, int size);
void get_scaling_factors(factor* L_factors, int &L_size, factor* M_factors, int &M_size);
void optimize_scaling_factors(scale* scales, int &S_size, factor* L_factors, int L_size, factor* M_factors, int M_size);

#endif

