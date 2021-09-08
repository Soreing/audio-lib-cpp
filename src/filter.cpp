#include <audio-lib/filter.h>
#include <string.h>
#include <math.h>

#define MATH_PI 3.1415f
#define SINC(fc, n) sin((2 * MATH_PI * fc * n)) / (MATH_PI * n)
#define HAMM(N, n)  0.54f + 0.46f * cos(n * 2 * MATH_PI / N)

FIRFilter_i64::FIRFilter_i64() :
	coefs(NULL), size(0)
{
}

FIRFilter_i64::FIRFilter_i64(int taps, int stop_freq, int sample_freq):
	coefs(NULL), size(0)
{	
	init(taps, stop_freq, sample_freq);
}

FIRFilter_i64::FIRFilter_i64(const FIRFilter_i64 &other):
	coefs(new llong[other.size]), size(other.size)

{	
	memcpy(coefs, other.coefs, sizeof(llong) * size);
}

FIRFilter_i64::~FIRFilter_i64()
{
	if(coefs != NULL)
	{	delete[] coefs;
	}
}

// Creates a low pass filter with "taps" number of coefficients
// Each coefficient is a 2^32 scaled up 64-bit integer value
void FIRFilter_i64::init(int taps, int stop_freq, int sample_freq)
{
	if(coefs != NULL)
	{	delete[] coefs;
	}
	
	size = taps;
	coefs = new llong[size];

	double ratio = (float)stop_freq / (float)sample_freq;
	double f_coef;

	for (int i = 0, p = i - size / 2; i < (int)size; i++, p++)
	{
		f_coef  = SINC(ratio, p);
		f_coef *= HAMM(size, p);
		coefs[i] = (llong)(f_coef * 4294967296);
	}

	coefs[size >> 1] = ((llong)(ratio * 4294967296)) << 1;
}