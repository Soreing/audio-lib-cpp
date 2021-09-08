#ifndef AUDIO_CONVERSION_H
#define AUDIO_CONVERSION_H

#include <audio-lib/wave.h>
#include <audio-lib/sampling.h>

struct factor
{	int value;
	int count;
};

struct scale
{	int L;
	int M;
};

class FormatConverter
{
public:
    int max_input;
    int max_output;

    WaveFmt in_fmt;
    WaveFmt out_fmt;

    char* channel_ptr;
    char* depth_ptr;
    char* rate_ptr;

    int L;
    int M; 

    int step_count;
    RateConverter* sub_steps;
    char** sub_buffers;

    FormatConverter(WaveFmt in, WaveFmt out);
    ~FormatConverter();

    int convert(char* src, char* dst, size_t blocks);
    int sub_convert(char* src, char* dst, size_t blocks);
};

int convert_sample_rate(const unsigned char* src, unsigned char* dst, size_t blocks, RateConverter &conv);
int convert_sample_rate(const short* src, short* dst, size_t blocks, RateConverter &conv);

void bit_depth_8_to_16(const unsigned char* src, short* dst, size_t samples);
void bit_depth_16_to_8(const short* src, unsigned char* dst, size_t samples);

void mono_to_stereo(const char* src, const char* dst, size_t depth, size_t samples);
void stereo_to_mono(const char* src, const char* dst, size_t depth, size_t samples);

int get_prime_factors(size_t value, factor* factors);
void remove_common_factors(factor* L_factors, const int L_size, factor* M_factors, const int M_size);
void optimize_scaling_factors(scale* scales, int &S_size, factor* L_factors, int L_size, factor* M_factors, int M_size);

#endif