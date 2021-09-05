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
	size_t L, M;

	get_convertion_ratio(44100, 48000, L, M);
	std::cout << "L= " << L << "   M= " << M << "\n";

	size_t LSize, MSize;
	size_t Ls[50], Ms[50];
	factorize(L, Ls, LSize);
	factorize(M, Ms, MSize);

	std::cout << "\nL= ";
	for(size_t i = 0; i < LSize; i++)
		std::cout << Ls[i] << ", ";

	std::cout << "\nM= ";
	for(size_t i = 0; i < MSize; i++)
		std::cout << Ms[i] << ", ";

	std::cout<< "\n";

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