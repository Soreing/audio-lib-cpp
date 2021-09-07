#include <audio-lib/sampling.h>
#include <audio-lib/primes.h>
#include <iostream>

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

int get_prime_factors(size_t value, factor* factors, int size)
{
	int new_size = 0;
	int prime_idx;
	int factor_idx;
	
	while(value > 1)
	{
		for(prime_idx = 0; prime_idx < PRIME_COUNT; prime_idx++)
		{	if(value % primes[prime_idx] == 0)
			{	value /= primes[prime_idx];
				break;
			}
		}

		if(prime_idx == PRIME_COUNT)
		{	return -1;
		}

		for(factor_idx = 0; factor_idx < new_size; factor_idx++)
		{	if(primes[prime_idx] == factors[factor_idx].value)
			{	factors[factor_idx].count++;
				break;
			}
		}

		if(factor_idx == new_size)
		{	if(new_size == size)
			{	return -1;
			}

			factors[factor_idx] = factor{primes[prime_idx], 1};
			new_size++;
		}
	}

	return new_size;
}

void get_scaling_factors(factor* L_factors, int &L_size, factor* M_factors, int &M_size)
{
	int L_newsize = 0;
	int M_newsize = 0;
	int i,j;

	for(i = 0, j = 0; i < L_size && j < M_size;)
	{
		if(L_factors[i].value < M_factors[j].value)
		{	L_factors[L_newsize++] = L_factors[i++];
		}
		else if(L_factors[i].value > M_factors[j].value)
		{	M_factors[M_newsize++] = M_factors[j++];
		}
		else
		{	
			if(L_factors[i].count < M_factors[j].count)
			{	M_factors[M_newsize] = M_factors[j];
				M_factors[M_newsize].count -= L_factors[i].count;
				M_newsize++;
			}
			else if(L_factors[i].count > M_factors[j].count)
			{	L_factors[L_newsize] = L_factors[i];
				L_factors[L_newsize].count -= M_factors[j].count;
				L_newsize++;
			}

			i++;
			j++;
		}
	}

	while(i < L_size)
	{	L_factors[L_newsize++] = L_factors[i++];
	}

	while(j < M_size)
	{	M_factors[M_newsize++] = M_factors[j++];
	}

	L_size = L_newsize;
	M_size = M_newsize;
}

void optimize_scaling_factors(scale* scales, int &S_size, factor* L_factors, int L_size, factor* M_factors, int M_size)
{
	size_t L_product=1;
	size_t M_product=1;
	
	int ratio;
	int S_newsize = 0;
	int i, j, k, L, M;

	for(i = L_size-1, j = M_size-1; i >= 0;)
	{
		if(L_factors[i].count == 0)
		{	i--;
		}
		else
		{
			L_factors[i].count--;
			L_product *= L_factors[i].value;

			L = L_factors[i].value;
			M = 1;

			while(true)
			{
				ratio = L_product / M_product;

				// Get the last factor of M
				while( j >= 0 && M_factors[j].count == 0)
				{	j--;
				}

				// Find a large factor to approach the ratio of 1
				for(k = j; k >= 0 ; k--)
				{	if(M_factors[k].value <= ratio && M_factors[k].count > 0)
					{	break;
					}
				}

				// If a factor is found, take it and add it to M and M_product
				if(k >= 0)
				{	M_factors[k].count--;
					M_product *= M_factors[k].value;
					M *= M_factors[k].value;
				}
				// If no more factors found and M is not 1, add pair
				else if( M != 1)
				{	scales[S_newsize] = scale{L, M};
					S_newsize++;
					break;
				}
				// If no factors are found at all, increase L by another small factor if exists
				else
				{
					// If there are no more factors in M, just add L
					if(j == -1)
					{	scales[S_newsize] = scale{L, 1};
						S_newsize++;
						break;
					}

					// Find the smallest factor
					for(k = 0; k <= i; k++)
					{	if(L_factors[k].count > 0)
						{	break;
						}
					}

					std::cout<< "K= " << k << "\n";

					// If valid, take and multiply the factor to L
					if(k <= i)
					{	L_factors[k].count--;
						L_product *= L_factors[k].value;
						L *= L_factors[k].value;
					}
					// Otherwise ran out of factors to multiply L with
					// Find the largest factor in M and take it
					else
					{	for(k = j; k >= 0 ; k--)
						{	if(M_factors[k].count > 0)
							{	break;
							}
						}

						M_factors[k].count--;
						scales[S_newsize] = scale{L, M_factors[k].value};
						S_newsize++;
						break;
					}
				}
			}
		}
	}

	while(j >= 0)
	{
		if(M_factors[j].count == 0)
		{	j--;
		}
		else
		{	M_factors[j].count--;
			scales[S_newsize] = scale{1, M_factors[j].value};
			std::cout<< "Added1\n"; 
			S_newsize++;
		}
	}

	S_size = S_newsize;
}