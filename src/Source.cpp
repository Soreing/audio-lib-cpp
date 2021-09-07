#include <iostream>
#include <iomanip>
#include <fstream>

#include <windows.h>

#include <audio-lib/AudioOutput.h>
#include <audio-lib/wave.h>
#include <audio-lib/sampling.h>
#include <audio-lib/conversion.h>

void getAudioClip(char* filename, char* &samples, int &size, WaveFmt &fmt)
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

	aout.setFormat(_Stereo, _16Bit, _44kHz);

	fmt1.bitsPerSample /= 2;
	fmt1.byteRate /= 2;
	fmt1.blockAlign /= 2;
	size1 /= 2;

	char* newClip = new char[size1];
	bit_depth_16_to_8((short*)clip1, (unsigned char*)newClip, size1 / (fmt1.bitsPerSample/8));

	fmt1.numChannels--;
	fmt1.byteRate /= 2;
	fmt1.blockAlign /= 2;
	size1 /= 2;

	char* newClip2 = new char[size1];
	stereo_to_mono(newClip, newClip2, 8, size1 / fmt1.blockAlign);

	mono_to_stereo(newClip2, newClip, 8, size1 / fmt1.blockAlign);

	fmt1.numChannels++;
	fmt1.byteRate *= 2;
	fmt1.blockAlign *= 2;
	size1 *= 2;

	bit_depth_8_to_16((unsigned char*)newClip, (short*)clip1, size1 / (fmt1.bitsPerSample/8));

	fmt1.bitsPerSample *= 2;
	fmt1.byteRate *= 2;
	fmt1.blockAlign *= 2;
	size1 *= 2;

	AudioSource* src1 = aout.createSource(fmt1, AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	src1->add(clip1, size1 / fmt1.blockAlign, fmt1);

	//AudioSource* src1 = aout.createSource(fmt1, AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	//src1->add(clip1, size1 / fmt1.blockAlign, fmt1);

	//AudioSource* src2 = aout.createSource(fmt2, AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	//src2->add(clip2, size2 / fmt2.blockAlign, fmt2);

	std::cout<< aout.openDevice(0);

	if (aout.state == Playing)
	{
		system("PAUSE");
		aout.closeDevice();
	}

	delete[] clip1;
	delete[] clip2;
}