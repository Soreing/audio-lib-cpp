#include <audio-lib/conversion.h>
#include <audio-lib/primes.h>
#include <string.h>
#include <math.h>

#define MIN(a, b) a < b ? a : b
#define MAX(a, b) a > b ? a : b

FormatConverter::FormatConverter(WaveFmt in, WaveFmt out) :
    in_fmt(in), out_fmt(out), sub_steps(NULL), sub_buffers(NULL),
    channel_ptr(NULL), depth_ptr(NULL), rate_ptr(NULL)
{
    // Maximum number of blocks processed by a converter
    max_input  = in.sampleRate/100;
    max_output = out.sampleRate/100;
    if(in.sampleRate % 100 != 0)
    {   max_input++;
        max_output++;
    }

    L = 1;  // Interpolation factor
    M = 1;  // Decimation factor

    // Allocate space for the channel conversion's output
    if(in_fmt.numChannels != out_fmt.numChannels)
    {   channel_ptr = new char[max_input * in_fmt.bitsPerSample/8 * out_fmt.numChannels ];
    }

    // Allocate space for the bit depth conversion's output
    if(in_fmt.bitsPerSample != out_fmt.bitsPerSample)
    {   depth_ptr = new char[max_input * out_fmt.byteRate];
    }

    // Set up Sample Rate Converters
    if(in_fmt.sampleRate != out_fmt.sampleRate)
    {   
        // Factorizing the input sample rate and the output sample rate,
        // Then dropping common factors to get a L/M fraction for conversion
        int in_fsize = get_prime_factors(in.sampleRate, 0);
        factor* in_factors = new factor[in_fsize];
        get_prime_factors(in.sampleRate, in_factors);

        int out_fsize = get_prime_factors(out.sampleRate, 0);
        factor* out_factors = new factor[out_fsize];
        get_prime_factors(out.sampleRate, out_factors);

        remove_common_factors(out_factors, out_fsize, in_factors, in_fsize);

        // Get the total number of factors to set the size of sub steps
        // The maximum number of sub steps can't exceed the number of factors
        int in_fcount  = 0;
        for(int i = 0; i < in_fsize; i++)
        {   M *= (int)pow(in_factors[i].value, in_factors[i].count);
            in_fcount += in_factors[i].count;
        }

        int out_fcount  = 0;
        for(int i = 0; i < out_fsize; i++)
        {   L *= (int)pow(out_factors[i].value, out_factors[i].count);
            out_fcount += out_factors[i].count;
        }

        step_count = MAX(in_fcount, out_fcount);
        scale* pairs = new scale[step_count];

        // Generate the series of L/M fractions to covnert the sampling rate
        // Create a series or Rate Converters to handle the conversion
        optimize_scaling_factors(pairs, step_count, out_factors, out_fsize, in_factors, in_fsize);
        sub_steps   = new RateConverter[step_count];
        sub_buffers = new char*[step_count];

        int tap_count;
        int buffer_size = max_input;
        for(int i = 0; i < step_count; i++)
        {   
            tap_count = (pairs[i].M * 12) | 1;
            sub_steps[i].init(pairs[i].L, pairs[i].M, tap_count, out_fmt.numChannels, out_fmt.bitsPerSample);
            
            buffer_size = (buffer_size * pairs[i].L / pairs[i].M) + 1;
            sub_buffers[i] = new char[buffer_size * out_fmt.blockAlign];
            memset(sub_buffers[i], 0, buffer_size * out_fmt.blockAlign);
        }

        max_output = buffer_size;
        rate_ptr = new char[max_output * out_fmt.byteRate];

        delete[] in_factors;
        delete[] out_factors;
        delete[] pairs;
    }
}

FormatConverter::~FormatConverter()
{
    if(channel_ptr != NULL)
    {   delete[] channel_ptr;
    }

    if(depth_ptr != NULL)
    {   delete[] depth_ptr;
    }

    if(rate_ptr != NULL)
    {   for(int i = 0; i < step_count; i++)
        {   delete[] sub_buffers[i];
        }

        delete[] rate_ptr;
        delete[] sub_steps;
        delete[] sub_buffers;
    }
}

void FormatConverter::convert(char* src, char* dst, size_t blocks)
{
    int size = 0;
    while(blocks > 0)
    {
        if((int)blocks > max_input)
        {   blocks -= max_input;
            size = sub_convert(src, dst, max_input);
            //std::cout << size << "\n";
            dst += size;
            src += max_input * in_fmt.blockAlign;
        }
        else
        {   blocks -= blocks;
            size = sub_convert(src, dst, max_input);
            //std::cout << size << "\n";
            dst += size;
            src += blocks * in_fmt.blockAlign;
        }
    }
}

int FormatConverter::sub_convert(char* src, char* dst, size_t blocks)
{
    char *channel_res, *depth_res, *rate_res;
    int output_blocks = blocks;

    // Change channel count if there is a mismatch
    if( in_fmt.numChannels != out_fmt.numChannels)
    {
        channel_res = channel_ptr;
        if(in_fmt.numChannels == 1)
        {   mono_to_stereo((char*)src, (char*)channel_res, in_fmt.bitsPerSample, blocks);
        }
        else if(in_fmt.numChannels == 2)
        {   stereo_to_mono((char*)src, (char*)channel_res, in_fmt.bitsPerSample, blocks);
        }
    }
    else
    {   channel_res = src;
    }

    // Change bit depth if there is a mismatch
    if( in_fmt.bitsPerSample != out_fmt.bitsPerSample)
    {
        depth_res = depth_ptr;
        if(in_fmt.bitsPerSample == 8)
        {   bit_depth_8_to_16((unsigned char*)channel_res, (short*)depth_res, blocks * out_fmt.numChannels);
        }
        else if(in_fmt.bitsPerSample == 16)
        {   bit_depth_16_to_8((short*)channel_res, (unsigned char*)depth_res, blocks * out_fmt.numChannels);
        }
    }
    else
    {   depth_res = channel_res;
    }

    // Change Sampling Frequency if there is a mismatch
    if( in_fmt.sampleRate != out_fmt.sampleRate)
    {
        char* sub_src;
        char* sub_dst = depth_res;

        for(int i = 0; i < step_count; i++)
        {
            sub_src = sub_dst;
            sub_dst = sub_buffers[i];

            switch(out_fmt.bitsPerSample)
            {   case 8:
                    output_blocks = convert_sample_rate(
                        (unsigned char*)sub_src, 
                        (unsigned char*)sub_dst, 
                        output_blocks, 
                        sub_steps[i]
                    );
                    break;
                case 16:
                    output_blocks = convert_sample_rate(
                        (short*)sub_src, 
                        (short*)sub_dst, 
                        output_blocks, 
                        sub_steps[i]
                    );
                    break;
            }
        }

        rate_res = sub_dst;
    }
    else
    {
        rate_res = depth_res;
    }

    memcpy(dst, rate_res, output_blocks * out_fmt.blockAlign);
    return output_blocks * out_fmt.blockAlign;
}

// Converts the sample rate of a multi channel 8-bit audio stream
// Returns the number of blocks extracted
int convert_sample_rate(const uchar* src, uchar* dst, size_t blocks, RateConverter &cnv)
{
    int ret = 0;

    if(cnv.L > 1 && cnv.M > 1)
    {   for(size_t j = 0; j < cnv.num_channels; j++)
        {   ret = cnv.non_integral((uchar*)src, (uchar*)dst, j, blocks);
        }
    }
    else if(cnv.L > 1)
    {   for(size_t j = 0; j < cnv.num_channels; j++)
        {   ret = cnv.interpolation((uchar*)src, (uchar*)dst, j, blocks);
        }
    }
    else if(cnv.M > 1)
    {   for(size_t j = 0; j < cnv.num_channels; j++)
        {   ret = cnv.decimation((uchar*)src, (uchar*)dst, j, blocks);
        }
    }

    return ret;
}

// Converts the sample rate of a multi channel 16-bit audio stream
// Returns the number of blocks extracted
int convert_sample_rate(const short* src, short* dst, size_t blocks, RateConverter &cnv)
{
    int ret = 0;

    if(cnv.L > 1 && cnv.M > 1)
    {   for(size_t j = 0; j < cnv.num_channels; j++)
        {   ret = cnv.non_integral((short*)src, (short*)dst, j, blocks);
        }
    }
    else if(cnv.L > 1)
    {   for(size_t j = 0; j < cnv.num_channels; j++)
        {   ret = cnv.interpolation((short*)src, (short*)dst, j, blocks);
        }
    }
    else if(cnv.M > 1)
    {   for(size_t j = 0; j < cnv.num_channels; j++)
        {   ret = cnv.decimation((short*)src, (short*)dst, j, blocks);
        }
    }

    return ret;
}

// Moves the samples from unsigned to signed wave and scales it up
void bit_depth_8_to_16(const unsigned char* src, short* dst, size_t samples)
{
    for(size_t i = 0; i < samples; i++)
    {   dst[i] = ((short)((char)(src[i] - 128))) << 8;
    }
}

// Scales the samples down and moves them from signed to unsigned wave
void bit_depth_16_to_8(const short* src, unsigned char* dst, size_t samples)
{
    for(size_t i = 0; i < samples; i++)
    {   dst[i] = ((char)(src[i] / 256)) + 128;
    }
}

// Duplicates samples to create 2 channels of the same wave
void mono_to_stereo(const char* src, const char* dst, size_t depth, size_t samples)
{
    for(size_t i = 0; i < samples; i++)
    {
        if(depth == 8)
        {   *((unsigned char*)dst + (i<<1))     = *((unsigned char*)src + i);
            *((unsigned char*)dst + (i<<1) + 1) = *((unsigned char*)src + i);
        }
        else if (depth == 16)
        {   *((short*)dst + (i<<1))     = *((short*)src + i);
            *((short*)dst + (i<<1) + 1) = *((short*)src + i);
        }
    }
}

// Averages 2 consecutive samples to combines 2 waves into 1
void stereo_to_mono(const char* src, const char* dst, size_t depth, size_t samples)
{
    for(size_t i = 0, sum = 0; i < samples; i++)
    {
        if(depth == 8)
        {   sum = *((unsigned char*)src + (i<<1)) + (int)*((unsigned char*)src + (i<<1) + 1);
            *((unsigned char*)dst + i) = (unsigned char)(sum >> 2);
        }
        else if (depth == 16)
        {   sum = *((short*)src + (i<<1)) + (int)*((short*)src + (i<<1) + 1);
            *((short*)dst + i) = (short)(sum >> 2);
        }
    }
}

// Finds the prime factors and their count of an integer value
// If called with factors=NULL, returns the space needed to store the result
int get_prime_factors(size_t value, factor* factors)
{
	int prime_idx   = 0;
	int factor_idx  = 0;
    int last_factor = 0;
	
	while(value > 1)
	{   
        // Find the next factor in the value
		for(; prime_idx < PRIME_COUNT; prime_idx++)
		{	if(value % primes[prime_idx] == 0)
			{   value /= primes[prime_idx];
				break;
			}
		}

        // If reached the end of the prime list and the value
        // is still not 1, the factor is too large to be found
		if(prime_idx == PRIME_COUNT && value > 1)
		{	return -1;
		}

        if(primes[prime_idx] != last_factor)
        {   // Set the last factor to the new one 
            // and add a new struct with count 1
            last_factor = primes[prime_idx];
            factor_idx++;
            if(factors != NULL)
            {   factors[factor_idx-1] = factor{last_factor, 1};
            }
        }
        else
        {   // Increment the count of the last factor
            if(factors != NULL)
            {   factors[factor_idx-1].count++;
            }
        }
	}

    return factor_idx;
}

// Removes the common factors between two numbers
void remove_common_factors(factor* L_factors, const int L_size, factor* M_factors, const int M_size)
{
    int i, j, min;
    for(i = 0, j = 0; i < L_size && j < M_size;)
	{
        // Case where L's value is less, L must be incremented
		if(L_factors[i].value < M_factors[j].value)
		{	i++;
		}
        // Case where M's value is less, M must be incremented
		else if(L_factors[i].value > M_factors[j].value)
		{	j++;
		}
        // Case when the factor values are identical
        // Subtract from both the amount that is common
		else
		{   min = MIN(L_factors[i].count, M_factors[j].count);
            L_factors[i++].count -= min;
            M_factors[j++].count -= min;
		}
	}
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
			S_newsize++;
		}
	}

	S_size = S_newsize;
}