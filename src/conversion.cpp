#include <audio-lib/conversion.h>
#include <audio-lib/primes.h>
#include <iostream>

#define MIN(a, b) a < b ? a : b
#define MAX(a, b) a > b ? a : b

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