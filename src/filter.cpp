#include "math.h"
#include <audio-lib/filter.h>

#define MATH_PI 3.1415f
#define SINC(fc, n) sin((2 * MATH_PI * fc * n)) / (MATH_PI * n)
#define HAMM(N, n)  0.54f + 0.46f * cos(n * 2 * MATH_PI / N)

FIRFilter_i64::FIRFilter_i64(int taps, int stop_freq, int sample_freq)
{
	size = taps;
	coefs = new llong[size];

	double ratio = (float)stop_freq / (float)sample_freq;
	double f_coef;

	for (size_t i = 0, p = i - size / 2; i < size; i++, p++)
	{
		f_coef = SINC(ratio, p);
		f_coef *= HAMM(size, p);
		coefs[i] = (llong)(f_coef * 4294967296);
	}

	coefs[size >> 1] = ((llong)(ratio * 4294967296)) << 1;
}