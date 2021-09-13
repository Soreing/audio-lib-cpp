#include <audio-lib/sampling.h>
#include <string.h>

#define MODINC(n, m) n = n == m-1 ? 0 : n+1;
#define MODDEC(n, m) n = n == 0 ? m-1 : n-1;
#define MODADD(n, v, m) n = (n + v) % m;
#define MODSUB(n, v, m) n = (n - v) % m;

RateConverter::RateConverter() :
	inter_delay_lines(0), inter_delay_idxs(0), inter_scales(0),
	decim_delay_lines(0), decim_delay_idxs(0), decim_fractions(0)
{
}

RateConverter::RateConverter(size_t L, size_t M, size_t taps, size_t channels, size_t depth) :
	inter_delay_lines(0), inter_delay_idxs(0),
	decim_delay_lines(0), decim_delay_idxs(0), decim_fractions(0)
{
	init(L, M, taps, channels, depth);
}

RateConverter::~RateConverter()
{
	clear();
}

// Initializes the filters and buffers for the converter
void RateConverter::init(size_t L, size_t M, size_t taps, size_t channels, size_t depth)
{
	this->L = L;
	this->M = M;

	this->num_channels = channels; 
	this->bit_depth = depth;

	inter_filter.init(taps, 1, L << 1);
	inter_delay_lines = new void*[channels];

	decim_filter.init(taps, 1, M << 1);
	decim_delay_lines = new void*[channels];

	inter_delay_idxs = new size_t[channels];
	decim_delay_idxs = new size_t[channels];
	decim_fractions  = new size_t[channels];

	memset(inter_delay_idxs, 0, sizeof(size_t) * channels);
	memset(decim_delay_idxs, 0, sizeof(size_t) * channels);
	memset(decim_fractions,  0, sizeof(size_t) * channels);

	// Create and initialize the delay lines for each channel
	for (size_t i = 0; i < channels; i++)
	{	inter_delay_lines[i] = new char[(bit_depth << 3) * taps / L];
		decim_delay_lines[i] = new char[(bit_depth << 3) * taps];

		// Set the default values according to unsigned (8-bit) or signed (16-bit+) zeros
		memset(inter_delay_lines[i], bit_depth == 8 ? 0x80 : 0, (bit_depth << 3) * taps / L);
		memset(decim_delay_lines[i], bit_depth == 8 ? 0x80 : 0, (bit_depth << 3) * taps);
	}

	// Add gain to the interpolation coefficients
	for(int i = 0; i < taps / L; i++)
	{	inter_filter.coefs[i] *= L;
	}

	// Calculate the scaling factor (to divide by) after decimation
	decim_scale = 0;
	for(size_t i = 0; i < taps ; i++)
	{	decim_scale += decim_filter.coefs[i];
	}

	// Calculate the scaling factor (to divide by) after interpolation
	// Each L samples use different coefficients, so they each have a scale factor
	inter_scales = new llong[L];
	int coef_orig = ((inter_filter.size >> 1) + 1) % L;
	int coef_idx;
	for(size_t i = 0; i < L; i++)
	{
		inter_scales[coef_orig] = 0;
		coef_idx = coef_orig;

		for(size_t j = 0; j < taps / L; j++)
		{	inter_scales[coef_orig] += inter_filter.coefs[coef_idx];
			coef_idx += L;
		}

		MODINC(coef_orig, L);
	}
}

// Deallocates all dynamic resources
void RateConverter::clear()
{
	if(inter_delay_lines != 0)
	{
		for (size_t i = 0; i < num_channels; i++)
		{	delete[] inter_delay_lines[i];
		}
		delete[] inter_delay_lines;
	}

	if(decim_delay_lines != 0)
	{
		for (size_t i = 0; i < num_channels; i++)
		{	delete[] decim_delay_lines[i];
		}
		delete[] decim_delay_lines;
	}

	if(inter_delay_idxs != 0) { delete[] inter_delay_idxs; }
	if(inter_scales != 0)     { delete[] inter_scales; }

	if(decim_delay_idxs != 0) { delete[] decim_delay_idxs; }
	if(decim_fractions != 0)  { delete[] decim_fractions; }
}

// Decimation of n 8-Bit "samples" from src to dst in a wave with some "channels"
int RateConverter::decimation(const uchar* src, uchar* dst, size_t channel, size_t samples)
{
	llong tmpVal;
	int m, n, count=0;

	size_t decim_fraction = decim_fractions[channel];
	size_t decim_delay_idx = decim_delay_idxs[channel];
	uchar* decim_delay_line = (uchar*)decim_delay_lines[channel];

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
			*dst = (uchar)(tmpVal /decim_scale);
			dst += num_channels;
			count++;
		}
	}

	decim_fractions[channel]  = decim_fraction;
	decim_delay_idxs[channel] = decim_delay_idx;
	return count;
}

// Interpolation of n 8-Bit "samples" from src to dst in a wave with some "channels"
int RateConverter::interpolation(const uchar* src, uchar* dst, size_t channel, size_t samples)
{
	size_t delay_size = inter_filter.size / L;				// Size of the delay line adjusted for interpolation
	size_t start_coef = ((inter_filter.size >> 1) + 1) % L;	// Coefficient index offset of the first sample
	size_t coef_idx   = start_coef;							// Index offset of the coefficients to use

	llong  tmpVal;
	int n, h, count=0;

	size_t inter_delay_idx = inter_delay_idxs[channel];
	uchar* inter_delay_line = (uchar*)inter_delay_lines[channel];

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
				tmpVal += inter_filter.coefs[h] * inter_delay_line[n];
				MODINC(n, delay_size);
				h += L;
			}

			// Add next sample from the accumulator to the destination
			// Divide the accumulator with the scale of the filter
			*dst = (uchar)(tmpVal / inter_scales[coef_idx]);
			dst += num_channels;
			count++;
			MODINC(coef_idx, L);
		}
	}

	inter_delay_idxs[channel] = inter_delay_idx;
	return count;
}

// Interpolation and Decimation of n 8-Bit "samples" from src to dst in a wave with some "channels"
int RateConverter::non_integral(const uchar* src, uchar* dst, size_t channel, size_t samples)
{
	size_t inter_size = inter_filter.size / L;				// Size of the delay line adjusted for interpolation
	size_t start_coef = ((inter_filter.size >> 1) + 1) % L;	// Coefficient index offset of the first sample
	size_t coef_idx = start_coef;							// Index offset of the coefficients to use

	llong tmpVal;
	int m, n, h, count=0;

	size_t decim_fraction = decim_fractions[channel];
	size_t decim_delay_idx = decim_delay_idxs[channel];
	uchar* decim_delay_line = (uchar*)decim_delay_lines[channel];

	size_t inter_delay_idx = inter_delay_idxs[channel];
	uchar* inter_delay_line = (uchar*)inter_delay_lines[channel];

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
				if (i == samples)
				{	
					decim_fractions[channel]  = decim_fraction;
					decim_delay_idxs[channel] = decim_delay_idx;
					inter_delay_idxs[channel] = inter_delay_idx;
					return count;
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
				tmpVal += inter_filter.coefs[h] * inter_delay_line[n];
				MODINC(n, inter_size);
				h += L;
			}

			// Add next sample from the accumulator to the destination
			// Divide the accumulator with the scale of the filter
			decim_delay_line[decim_delay_idx] = (uchar)(tmpVal / inter_scales[coef_idx]);
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
		*dst = (uchar)(tmpVal  / decim_scale);
		dst += num_channels;
		count++;
	}

	return 0;
}


// Decimation of n 16-Bit "samples" from src to dst in a wave with some "channels"
int RateConverter::decimation(const short* src, short* dst, size_t channel, size_t samples)
{
	llong tmpVal;
	int m, n, count=0;

	size_t decim_fraction = decim_fractions[channel];
	size_t decim_delay_idx = decim_delay_idxs[channel];
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
			*dst = (short)(tmpVal / decim_scale);
			dst += num_channels;
			count++;
		}
	}

	decim_fractions[channel]  = decim_fraction;
	decim_delay_idxs[channel] = decim_delay_idx;
	return count;
}

// Interpolation of n 16-Bit "samples" from src to dst in a wave with some "channels"
int RateConverter::interpolation(const short* src, short* dst, size_t channel, size_t samples)
{
	size_t delay_size = inter_filter.size / L;				// Size of the delay line adjusted for interpolation
	size_t start_coef = ((inter_filter.size >> 1) + 1) % L;	// Coefficient index offset of the first sample
	size_t coef_idx   = start_coef;							// Index offset of the coefficients to use

	llong  tmpVal;
	int n, h, count=0;

	size_t inter_delay_idx = inter_delay_idxs[channel];
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
				tmpVal += inter_filter.coefs[h] * inter_delay_line[n];
				MODINC(n, delay_size);
				h += L;
			}

			// Add next sample from the accumulator to the destination
			// Divide the accumulator with the scale of the filter
			*dst = (short)(tmpVal / inter_scales[coef_idx]);
			dst += num_channels;
			count++;
			MODINC(coef_idx, L);
		}
	}

	inter_delay_idxs[channel] = inter_delay_idx;
	return count;
}

// Interpolation and Decimation of n 16-Bit "samples" from src to dst in a wave with some "channels"
int RateConverter::non_integral(const short* src, short* dst, size_t channel, size_t samples)
{
 
	size_t inter_size = inter_filter.size / L;				// Size of the delay line adjusted for interpolation
	size_t start_coef = ((inter_filter.size >> 1) + 1) % L;	// Coefficient index offset of the first sample
	size_t coef_idx = start_coef;							// Index offset of the coefficients to use

	llong tmpVal;
	int m, n, h, count=0;

	size_t decim_fraction = decim_fractions[channel];
	size_t decim_delay_idx = decim_delay_idxs[channel];
	short* decim_delay_line = (short*)decim_delay_lines[channel];

	size_t inter_delay_idx = inter_delay_idxs[channel];
	short* inter_delay_line = (short*)inter_delay_lines[channel];

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
				if (i == samples)
				{	
					decim_fractions[channel]  = decim_fraction;
					decim_delay_idxs[channel] = decim_delay_idx;
					inter_delay_idxs[channel] = inter_delay_idx;
					
					return count;
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
				tmpVal += inter_filter.coefs[h] * inter_delay_line[n];
				MODINC(n, inter_size);
				h += L;
			}

			// Add next sample from the accumulator to the destination
			// Divide the accumulator with the scale of the filter
			decim_delay_line[decim_delay_idx] = (short)(tmpVal / inter_scales[coef_idx]);
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
		*dst = (short)(tmpVal / decim_scale);
		dst += num_channels;
		count++;
	}

	return 0;
}