#include <audio-lib/wave.h>
#include <string.h>

bool operator==(const WaveFmt &a, const WaveFmt &b)
{
    return  a.numChannels   == b.numChannels   &&
		    a.bitsPerSample == b.bitsPerSample &&
		    a.sampleRate    == b.sampleRate;
}

bool operator!=(const WaveFmt &a, const WaveFmt &b)
{
    return  !(a == b);
}

WaveFmt makeWaveFmt(short numChannels, short bitsPerSample, long sampleRate)
{
    WaveFmt fmt;
    fmt.audioFormat   = 1;
    fmt.numChannels   = numChannels;
    fmt.bitsPerSample = bitsPerSample;
    fmt.sampleRate    = sampleRate;
    fmt.blockAlign    = numChannels * bitsPerSample/8;
    fmt.byteRate      = fmt.blockAlign * sampleRate;

    return fmt;
}

bool isCorrectHeader(WAVEHeader &hdr)
{
    if(memcmp(hdr.chunkID, "RIFF", 4) == 0)
    {   
        if(memcmp(hdr.format, "WAVE", 4) == 0)
        {   
            if(memcmp(hdr.subchunk1ID, "fmt ", 4) == 0)
            {   
                if(memcmp(hdr.subchunk2ID, "data", 4) == 0)
                {   
                    int expectedSize = 4 + (8 + hdr.subchunk1Size) + (8 + hdr.subchunk2Size);
                    if(hdr.subchunk1Size == 16 && hdr.chunkSize == expectedSize)
                    {   
                        int expectedAlign    = hdr.wfmt.numChannels * hdr.wfmt.bitsPerSample/8;
                        int expectedByteRate = expectedAlign * hdr.wfmt.sampleRate;
                        if(hdr.wfmt.blockAlign == expectedAlign && hdr.wfmt.byteRate == expectedByteRate)
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}