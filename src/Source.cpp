#include <iostream>
#include <iomanip>
#include <fstream>

#include <windows.h>

#include <audio-lib/AudioOutput.h>
#include <audio-lib/wave.h>

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

	aout.setFormat(_Stereo, _16Bit, _22kHz);

	AudioSource* src1 = aout.createSource(fmt1, AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	src1->add(clip1, size1 / fmt1.blockAlign, fmt1);

	AudioSource* src2 = aout.createSource(fmt2, AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	src2->add(clip2, size2 / fmt2.blockAlign, fmt2);

	aout.openDevice(0);

	if (aout.state == Playing)
	{
		system("PAUSE");
		aout.closeDevice();
	}

	delete[] clip1;
	delete[] clip2;
}