#ifndef FILTER_H
#define FILTER_H

typedef long long llong;

class FIRFilter_i64
{
public:
	llong* coefs;	// Filter coefficients
	size_t size;	// Number of filter coefficients

public:
	FIRFilter_i64();
	FIRFilter_i64(int taps, int stop_freq, int sample_freq);
	FIRFilter_i64(const FIRFilter_i64 &other);
	~FIRFilter_i64();

	// Creates a low pass filter with "taps" number of coefficients
	// Each coefficient is a 2^32 scaled up 64-bit integer value
	void init(int taps, int stop_freq, int sample_freq);
};

#endif