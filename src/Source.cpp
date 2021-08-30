#include <iostream>
#include <iomanip>
#include <fstream>

#include <windows.h>

#include <audio-lib/AudioOutput.h>
#include <audio-lib/wave.h>

int main()
{
	AudioOutput ostr;
	
	WAVEFORMATEX format;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = 44100;
	format.wBitsPerSample = 16;
	format.nBlockAlign = 4;
	format.nAvgBytesPerSec = 176400;
	format.cbSize = 0;

	ostr.supported_fmt = format;
	if (ostr.openDevice(0) == 0)
	{	
		std::cout << "Playing...\n";

		WAVEHeader wav;
		std::ifstream in("test.wav", std::ios::binary);
		in.read((char*)&wav, sizeof(WAVEHeader));

		char* samples = new char[wav.subchunk2Size];
		in.read(samples, wav.subchunk2Size);

		AudioSource* src = ostr.createSource(format, AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
		src->add(samples, wav.subchunk2Size / wav.blockAlign, format);
	}

	for (;;);
}