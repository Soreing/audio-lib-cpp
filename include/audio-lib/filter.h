#ifndef FILTER_H
#define FILTER_H

typedef long long llong;

class FIRFilter_i64
{
public:
	llong* coefs;
	size_t size;

public:
	FIRFilter_i64(int taps, int stop_freq, int sample_freq);
};

#endif