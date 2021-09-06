#include <iostream>
#include <iomanip>
#include <fstream>

#include <windows.h>

#include <audio-lib/AudioOutput.h>
#include <audio-lib/wave.h>
#include <audio-lib/sampling.h>

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
	factor Lfs[10], Mfs[10];
	int _48size = get_prime_factors(48000, Lfs, 10);
	int _44size = get_prime_factors(44100, Mfs, 10);

	std::cout<< "All:\nL= ";
	for(int i=0; i<_48size; i++)
	{	std::cout <<  Lfs[i].value << "^" << Lfs[i].count << "     ";
	}	std::cout<< "\n";

	std::cout<< "M= ";
	for(int i=0; i<_44size; i++)
	{	std::cout <<  Mfs[i].value << "^" << Mfs[i].count << "     ";
	}	std::cout<< "\n";


	get_scaling_factors(Lfs, _48size, Mfs, _44size);

	std::cout<< "\nUncommon:\nL= ";
	for(int i=0; i<_48size; i++)
	{	std::cout <<  Lfs[i].value << "^" << Lfs[i].count << "     ";
	}	std::cout<< "\n";

	std::cout<< "M= ";
	for(int i=0; i<_44size; i++)
	{	std::cout <<  Mfs[i].value << "^" << Mfs[i].count << "     ";
	}	std::cout<< "\n";

	system("PAUSE");

	AudioOutput  aout;
	
	char   *clip1, *clip2;
	int     size1,  size2;
	WaveFmt fmt1,   fmt2; 

	getAudioClip("clip1.wav", clip1, size1, fmt1);
	getAudioClip("clip2.wav", clip2, size2, fmt2);

	aout.setFormat(_Stereo, _16Bit, _44kHz);

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