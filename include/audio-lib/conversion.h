#ifndef AUDIO_CONVERSION_H
#define AUDIO_CONVERSION_H

#include <audio-lib/wave.h>
#include <audio-lib/sampling.h>

struct factor
{	int value;                  // Value of the prime factor
	int count;                  // Count of the prime factor present
};

struct scale
{	int L;                      // Interpolation factor
	int M;                      // Decimation factor
};

class FormatConverter
{
public:
    int max_input;              // Maximum number of blocks processed by the sub_conversion
    int max_output;             // Maximum number of blocks output by the sub_conversion

    WaveFmt in_fmt;             // Input wave's format
    WaveFmt out_fmt;            // Output wave's format

    char* channel_ptr;          // Temporary dynamic array for the channel conversion's result
    char* depth_ptr;            // Temporary dynamic array for the bit depth conversion's result
    char* rate_ptr;             // Temporary dynamic array for the sampling rate conversion's result

    int L;                      // Total Interpolation factor
    int M;                      // Total Decimation factor

    int step_count;             // Number of sub-steps for the sampling rate conversion
    char** sub_buffers;         // Intermediate buffers for sampling rate conversion
    RateConverter* sub_steps;   // Smaller increments of Rate Converters to convert the sampling rate of the input
    

    FormatConverter(WaveFmt in, WaveFmt out);
    ~FormatConverter();

    // Breaks down the conversion of a larger wave to smaller steps
    // Returns the number of blocks that resulted from the conversion
    int convert(char* src, char* dst, size_t blocks);

    // Converts a wave to another format, which can include different
    // number of channels, nit depth or sampling rate increase or decrease
    // Returns the number of blocks that resulted from the conversion
    int sub_convert(char* src, char* dst, size_t blocks);
};

// Converts the sample rate of a multi channel 8-bit audio stream
int convert_sample_rate(const unsigned char* src, unsigned char* dst, size_t blocks, RateConverter &conv);

// Converts the sample rate of a multi channel 16-bit audio stream
int convert_sample_rate(const short* src, short* dst, size_t blocks, RateConverter &conv);



// Moves the samples from unsigned 8-bit to signed 16-bit wave and scales it up
void bit_depth_8_to_16(const unsigned char* src, short* dst, size_t samples);

// Scales the signed 16-bit samples down and moves them to an unsigned 8-bit sample wave
void bit_depth_16_to_8(const short* src, unsigned char* dst, size_t samples);



// Duplicates samples to create 2 channels of the same wave
void mono_to_stereo(const char* src, const char* dst, size_t depth, size_t samples);

// Averages 2 consecutive samples to combines 2 waves into 1
void stereo_to_mono(const char* src, const char* dst, size_t depth, size_t samples);



// Finds the prime factors and their count of an integer value
// If called with factors=NULL, returns the space needed to store the result
int get_prime_factors(size_t value, factor* factors);

// Removes the common factors between two numbers
void remove_common_factors(factor* L_factors, const int L_size, factor* M_factors, const int M_size);

// Optimizes the scaling factors L/M bteween two sampling rates so
// the wave doesn't suffer quality loss during conversion and the
// conversion is performed as fast as possible with minimized memory usage
void optimize_scaling_factors(scale* scales, int &S_size, factor* L_factors, int L_size, factor* M_factors, int M_size);

#endif