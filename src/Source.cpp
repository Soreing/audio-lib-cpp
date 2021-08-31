#include <iostream>
#include <iomanip>
#include <fstream>

#include <windows.h>

#include <audio-lib/AudioOutput.h>
#include <audio-lib/wave.h>

int main()
{
	AudioOutput ostr;
	
	// Setting Audio Format
	WAVEFORMATEX format;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = 44100;
	format.wBitsPerSample = 16;
	format.nBlockAlign = 4;
	format.nAvgBytesPerSec = 176400;
	format.cbSize = 0;

	ostr.supported_fmt = format;

	// Open speaker device 0 (default)

	int res;
	if ((res = ostr.openDevice(0)) == 0)
	{	
		std::cout << "Playing...\n";

		// Open and read test .wav audio file and extract header
		WAVEHeader wav;
		std::ifstream in("test.wav", std::ios::binary);
		in.read((char*)&wav, sizeof(WAVEHeader));

		// Extract audio data from the test file
		char* samples = new char[wav.subchunk2Size];
		in.read(samples, wav.subchunk2Size);

		// Create an AudioSource on the speaker and add the audio data from the file
		AudioSource* src = ostr.createSource(format, AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
		src->add(samples, wav.subchunk2Size / wav.blockAlign, format);
	}

	system("PAUSE");

	ostr.closeDevice();

	system("PAUSE");
}