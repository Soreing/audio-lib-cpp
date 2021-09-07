#include <audio-lib/conversion.h>
#include <audio-lib/primes.h>

// Moves the samples from unsigned to signed wave and scales it up
void bit_depth_8_to_16(const unsigned char* src, short* dst, size_t samples)
{
    for(size_t i = 0; i < samples; i++)
    {   dst[i] = ((short)((char)(src[i] - 127))) << 8;
    }
}

// Scales the samples down and moves them from signed to unsigned wave
void bit_depth_16_to_8(const short* src, unsigned char* dst, size_t samples)
{
    for(size_t i = 0; i < samples; i++)
    {   dst[i] = ((char)(src[i] >> 8)) + 127;
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