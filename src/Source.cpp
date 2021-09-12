#include <iostream>
#include <iomanip>
#include <fstream>
#include <conio.h>

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
	
	char   *clip1, *clip2, *clip3;
	int     size1,  size2,  size3;
	WaveFmt fmt1,   fmt2,   fmt3; 

	getAudioClip("clip1.wav", clip1, size1, fmt1);
	getAudioClip("clip2.wav", clip2, size2, fmt2);
	getAudioClip("clip3.wav", clip3, size3, fmt3);

	aout.setFormat(makeWaveFmt(_Stereo, _16Bit, _48kHz));

	AudioSource* src1 = aout.createSource(AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	src1->add_async(clip1, size1 / fmt1.blockAlign, fmt1);
	src1->add_async(clip2, size2 / fmt2.blockAlign, fmt2);
	src1->add_async(clip3, size3 / fmt3.blockAlign, fmt3);

	//AudioSource* src2 = aout.createSource(AS_FLAG_BUFFERED | AS_FLAG_LOOPED);
	//src2->add_async(clip2, size2 / fmt2.blockAlign, fmt2);

	aout.openDevice(0);
	/*if (aout.state == Playing)
	{
		system("PAUSE");
		aout.setFormat(makeWaveFmt(_Stereo, _16Bit, _11kHz));
		system("PAUSE");
		aout.closeDevice();
	}*/

	char* op_name[3]  = {"Channels:      ", "Bit-Depth:     ", "Sampling Rate: "}; 
	int*  op_val [3];

	int op_channel[2] = { _Mono, _Stereo }; 
	int op_depth[2]   = { _8Bit, _16Bit }; 
	int op_rate[10]   = { _8kHz, _11kHz, _16kHz, _22kHz, _24kHz, _32kHz, _44kHz, _48kHz, _88kHz, _96kHz };

	int max_op = 2;
	int select_op = 0;

	int select_val[3] = { 1, 1, 6 };
	int select_max[3] = { 1, 1, 9 };

	op_val[0] = op_channel;
	op_val[1] = op_depth;
	op_val[2] = op_rate;

system("PAUSE");
	system("CLS");
	for(int i=0; i<=max_op; i++)
	{	std::cout<< (i==select_op ? " > " : "   ") << op_name[i] << op_val[i][select_val[i]] << "\n";
	}

	for(;;)
	{
		char c = getch();
		switch(c)
		{
			case 'w':  select_op--; break;
			case 's':  select_op++; break;
			case 'a':  select_val[select_op]--; break;
			case 'd':  select_val[select_op]++; break;
		}

		select_op = select_op < 0 ? 0 : select_op > max_op ? max_op : select_op;
		select_val[select_op] = select_val[select_op] < 0 ? 0 : select_val[select_op] > select_max[select_op] ? select_max[select_op] : select_val[select_op];

		system("CLS");
		for(int i=0; i<=max_op; i++)
		{	std::cout<< (i == select_op ? " > " : "   ") << op_name[i] << op_val[i][select_val[i]] << "\n";
		}

		if(c == 'a' || c =='d')
			aout.setFormat(makeWaveFmt(op_val[0][select_val[0]], op_val[1][select_val[1]], op_val[2][select_val[2]]));
	}




	delete[] clip1;
	delete[] clip2;

	return 0;
}