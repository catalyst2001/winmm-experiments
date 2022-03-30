// 
// PCM WAV resample demo
// Author: Deryabin K.
// Date: 29.03.2022
// 
#include "../common/wav.h"
#include <mmsystem.h>

wave_t wav;

int main()
{
	if (!load_wave(&wav, "1.wav")) {
		printf("Failed to load wave sound!\n");
		return 1;
	}



	free_wave(&wav);
	return 0;
}