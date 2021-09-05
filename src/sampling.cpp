#include <audio-lib/sampling.h>

#define MODINC(n, m) n = n == m-1 ? 0 : n+1;
#define MODDEC(n, m) n = n == 0 ? m-1 : n-1;
#define MODADD(n, v, m) n = (n + v) % m;
#define MODSUB(n, v, m) n = (n - v) % m;

Resampler::Resampler(size_t L, size_t M, size_t taps, size_t channels, size_t depth) :
	L(L), M(M), num_channels(channels), bit_depth(depth),
	inter_filter(taps, 1, L << 1), inter_delay_idx(0), inter_delay_lines(new void*[channels]),
	decim_filter(taps, 1, M << 1), decim_delay_idx(0), decim_delay_lines(new void*[channels]),
	decim_fraction(0)
{
	for (size_t i = 0; i < channels; i++)
	{
		inter_delay_lines[i] = new char[(bit_depth << 3) * taps / L];
		decim_delay_lines[i] = new char[(bit_depth << 3) *taps];
	}
}

Resampler::~Resampler()
{
	for (size_t i = 0; i < num_channels; i++)
	{
		delete[] inter_delay_lines[i];
		delete[] decim_delay_lines[i];
	}

	delete[] inter_delay_lines;
	delete[] decim_delay_lines;
}

void Resampler::decimation_16(const short* src, short* dst, size_t channel, size_t samples)
{
	llong tmpVal;
	int m, n;

	short* decim_delay_line = (short*)decim_delay_lines[channel];
	src += channel;	// Ptr of next sample in the source (current channel)
	dst += channel;	// Ptr of next sample in the destination (current channel)

	for (size_t i = 0; i < samples; i++)
	{
		// Add one sample to the delay line
		decim_delay_line[decim_delay_idx] = *src;
		src += num_channels;
		MODINC(decim_delay_idx, decim_filter.size);
		MODINC(decim_fraction, M);

		// If enough samples have been added from the source to the delay line, 
		// derive one sample and add it to the destination pointer
		if (decim_fraction == 0)
		{
			tmpVal = 0;
			m = decim_delay_idx;
			n = decim_delay_idx;
			MODDEC(n, decim_filter.size);

			// Perform convolution between impulse response coefficients from the filter
			// and the delay line sample values at the symmetric ends of the response
			for (size_t k = 0; k < (decim_filter.size >> 1); k++)
			{
				tmpVal += decim_filter.coefs[k] * ((llong)decim_delay_line[m] + (llong)decim_delay_line[n]);
				MODINC(m, decim_filter.size);
				MODDEC(n, decim_filter.size);
			}

			// If the delay line in odd, the middle value is added separately
			if (decim_filter.size & 1)
			{
				tmpVal += decim_filter.coefs[decim_filter.size >> 1] * decim_delay_line[n];
			}

			// Divide the accumulator with the scale of the filter
			*dst = (short)(tmpVal >> 32);
			dst += num_channels;
		}
	}
}

void Resampler::interpolation_16(const short* src, short* dst, size_t channel, size_t samples)
{
	size_t delay_size = inter_filter.size / L;				// Size of the delay line adjusted for interpolation
	size_t start_coef = ((inter_filter.size >> 1) + 1) % L;	// Coefficient index offset of the first sample
	size_t coef_idx   = start_coef;							// Index offset of the coefficients to use

	llong  tmpVal;
	int n, h;

	short* inter_delay_line = (short*)inter_delay_lines[channel];
	src += channel;	// Ptr of next sample in the source (current channel)
	dst += channel;	// Ptr of next sample in the destination (current channel)

	for (size_t i = 0; i < samples; i++)
	{
		// Add next samples to the delay line
		inter_delay_line[inter_delay_idx] = *(src);
		src += num_channels;
		MODINC(inter_delay_idx, delay_size);

		// Calculate L-1 and the real sample with a lowpass filter
		for (size_t j = 0; j < L; j++)
		{
			tmpVal = 0;
			n = inter_delay_idx;
			h = coef_idx;

			// Perform convolution between impulse response coefficients from the filter
			// and the delay line sample values to interpolate the zero samples
			// The coefficients are multiplied by the factor to add gain
			for (size_t k = 0; k < delay_size; k++)
			{
				tmpVal += inter_filter.coefs[h] * L * inter_delay_line[n];
				MODINC(n, delay_size);
				h += L;
			}

			// Add next sample from the accumulator to the destination
			// Divide the accumulator with the scale of the filter
			*dst = (short)(tmpVal >> 32);
			dst += num_channels;
			MODINC(coef_idx, L);
		}
	}
}

void Resampler::non_integral_16(const short* src, short* dst, size_t channel, size_t samples)
{
	size_t inter_size = inter_filter.size / L;				// Size of the delay line adjusted for interpolation
	size_t start_coef = ((inter_filter.size >> 1) + 1) % L;	// Coefficient index offset of the first sample
	size_t coef_idx = start_coef;							// Index offset of the coefficients to use

	llong tmpVal;
	int m, n, h;

	short* inter_delay_line = (short*)inter_delay_lines[channel];
	short* decim_delay_line = (short*)decim_delay_lines[channel];
	src += channel;	// Ptr of next sample in the source (current channel)
	dst += channel;	// Ptr of next sample in the destination (current channel)

	for (size_t i = 0;;)
	{
		// Calculate L-1 and the real sample with a lowpass filter
		for (size_t j = decim_fraction; j < M; j++)
		{
			// Add new sample if the previous has been fully interpolated
			if (coef_idx == start_coef)
			{
				// If the samples ended, quit the function
				if (i == samples - 1)
				{
					return;
				}

				inter_delay_line[inter_delay_idx] = *src;
				MODINC(inter_delay_idx, inter_size);
				src += num_channels;
				i++;
			}

			tmpVal = 0;
			n = inter_delay_idx;
			h = coef_idx;

			// Perform convolution between impulse response coefficients from the filter
			// and the delay line sample values to interpolate the zero samples
			// The coefficients are multiplied by the factor to add gain
			for (size_t k = 0; k < inter_size; k++)
			{
				tmpVal += inter_filter.coefs[h] * L * inter_delay_line[n];
				MODINC(n, inter_size);
				h += L;
			}

			// Add next sample from the accumulator to the destination
			// Divide the accumulator with the scale of the filter
			decim_delay_line[decim_delay_idx] = (short)(tmpVal >> 32);
			MODINC(decim_delay_idx, decim_filter.size);
			MODINC(decim_fraction, M);
			MODINC(coef_idx, L);
		}

		tmpVal = 0;
		m = decim_delay_idx;
		n = decim_delay_idx;
		MODDEC(n, decim_filter.size);

		// Perform convolution between impulse response coefficients from the filter
		// and the delay line sample values at the symmetric ends of the response
		for (size_t k = 0; k < (decim_filter.size >> 1); k++)
		{
			tmpVal += decim_filter.coefs[k] * ((llong)decim_delay_line[m] + (llong)decim_delay_line[n]);
			MODINC(m, decim_filter.size);
			MODDEC(n, decim_filter.size);
		}

		// If the delay line in odd, the middle value is added separately
		if (decim_filter.size & 1)
		{
			tmpVal += decim_filter.coefs[decim_filter.size >> 1] * decim_delay_line[n];
		}

		// Divide the accumulator with the scale of the filter
		*dst = (short)(tmpVal >> 32);
		dst += num_channels;
	}
}