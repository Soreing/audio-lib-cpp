#include <iostream>
#include <iomanip>
#include <fstream>

#include <windows.h>

#include <audio-lib/AudioOutput.h>
#include <audio-lib/wave.h>
#include <audio-lib/sampling.h>
#include <audio-lib/conversion.h>

void putAudioClip(const char* filename, const char* samples, const int size, const WaveFmt fmt)
{
	WAVEHeader wav;

	wav.subchunk2Size = size;
	wav.wfmt = fmt;
	wav.chunkSize = 4 + (8 + wav.subchunk1Size) + (8 + wav.subchunk2Size);

	std::ofstream out(filename, std::ios::binary);
	out.write((char*)&wav, sizeof(WAVEHeader));
	out.write(samples, wav.subchunk2Size);
	out.close();
}

void getAudioClip(const char* filename, char* &samples, int &size, WaveFmt &fmt)
{
	WAVEHeader wav;

	std::ifstream in(filename, std::ios::binary);
	in.read((char*)&wav, sizeof(WAVEHeader));

	samples = new char[wav.subchunk2Size];
	in.read(samples, wav.subchunk2Size);
	in.close();

	size = wav.subchunk2Size;
	fmt = wav.wfmt;
}

int main()
{
	AudioOutput  aout;
	
	char   *clip1, *clip2;
	int     size1,  size2;
	WaveFmt fmt1,   fmt2; 

	getAudioClip("clip1.wav", clip1, size1, fmt1);
	getAudioClip("clip2.wav", clip2, size2, fmt2);

	aout.setFormat(makeWaveFmt(_Stereo, _16Bit, _44kHz));
	aout.openDevice(0);

	AudioSource* src1 = aout.createSource(AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	src1->add_async(clip1, size1 / fmt1.blockAlign, fmt1);

	AudioSource* src2 = aout.createSource(AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	src2->add_async(clip2, size2 / fmt2.blockAlign, fmt2);

	if (aout.state == Playing)
	{
		system("PAUSE");
		aout.closeDevice();
	}

	delete[] clip1;
	delete[] clip2;
}