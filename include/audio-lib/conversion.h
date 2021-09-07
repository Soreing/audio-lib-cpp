#ifndef AUDIO_CONVERSION_H
#define AUDIO_CONVERSION_H

struct factor
{	int value;
	int count;
};

struct scale
{	int L;
	int M;
};

int get_prime_factors(size_t value, factor* factors, int size);
void get_scaling_factors(factor* L_factors, int &L_size, factor* M_factors, int &M_size);
void optimize_scaling_factors(scale* scales, int &S_size, factor* L_factors, int L_size, factor* M_factors, int M_size);

#endif