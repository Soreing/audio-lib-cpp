#ifndef WAVE_H
#define WAVE_H

// Information taken from online at:
// http://soundfile.sapp.org/doc/WaveFormat/

enum Channel    { _Mono=1,  _Stereo=2 };
enum SampleSize { _8Bit=8, _16Bit=16  };
enum Frequency  { _11kHz=11025, _22kHz=22050, _44kHz=44100, _48kHz=48000, _96kHz=96000};

struct WaveFmt
{   
    short audioFormat;                          // PCM = 1 Values other than 1 indicate some form of compression
    short numChannels;                          // Mono = 1, Stereo = 2, etc
    long  sampleRate;                           // 8000, 44100, etc.
    long  byteRate;                             // SampleRate * NumChannels * BitsPerSample/8
    short blockAlign;                           // NumChannels * BitsPerSample/8
    short bitsPerSample;                        // 8 bits = 8, 16 bits = 16, etc
};

struct WAVEHeader
{
    char  chunkID[4] = {'R','I','F','F'};       // Contains the letters "RIFF" in ASCII form
    long  chunkSize;                            // 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
    char  format[4]  = {'W','A','V','E'};       // Contains the letters "WAVE" in ASCII form

    char  subchunk1ID[4] = {'f','m','t',' '};   // Contains the letters "fmt " in ASCII form
    long  subchunk1Size  = 16;                  // 16 for PCM
    WaveFmt wfmt;                               // Wave format structure

    char  subchunk2ID[4] = {'d','a','t','a'};   // Contains the letters "data" in ASCII form
    long  subchunk2Size;                        // This is the number of bytes in the data
};

WaveFmt makeWaveFmt(short numChannels, short bitsPerSample, long sampleRate);

bool isCorrectHeader(WAVEHeader &hdr);

#endif
