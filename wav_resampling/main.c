// 
// PCM WAV resample demo
// Author: Deryabin K.
// Date: 29.03.2022
// 
// downsampling / upsampling
// 
#include "../common/wav.h"
#include <mmsystem.h>
#include <math.h>
#include <limits.h>

wave_t wav;

typedef struct sndbuffer_s {
	sndformat2_t format;
	size_t buffer_size;
} sndbuffer_t;

// 
// convert_bitrate
// 
// Convert sound samples bitrate
// 
bool convert_bitrate(short *p_dest_buffer, size_t dest_size, void *p_src_samples, size_t src_size, size_t sample_rate, short bits_per_sample, short number_of_channels)
{
	unsigned char *p_src = (unsigned char *)p_src_samples;
	if (number_of_channels < 1 || number_of_channels > 2)
		return false;

	int samples[2];
	short bytes_per_sample = bits_per_sample / 8;
	short channel_offset = number_of_channels - 1;
	int number_of_samples = src_size / (bytes_per_sample * number_of_channels);
	float max_range = powf(2.f, (float)bits_per_sample);
	for (int i = 0; i < number_of_samples;) {
		if (number_of_channels == 2) {
			memcpy(&samples[0], &p_src[i * bytes_per_sample], bytes_per_sample);
			memcpy(&samples[1], &p_src[i * bytes_per_sample * channel_offset], bytes_per_sample);
		}
		else if (number_of_channels == 1) {
			memcpy(&samples[0], &p_src[i * bytes_per_sample], bytes_per_sample);
			memcpy(&samples[1], &p_src[i * bytes_per_sample], bytes_per_sample);
		}
		p_dest_buffer[i++] = ((float)samples[0] / max_range) * 65535.f;
		p_dest_buffer[i++] = ((float)samples[1] / max_range) * 65535.f;
	}
	return true;
}

int downsample_to_s16(short *p_dest_samples_buf, size_t *p_dest_nsamples, int dest_samplerate, int dest_channels, const void *p_src_samples, const int n_src_channels, const int n_src_bitspersample, const size_t src_samples_buffer_size)
{
	if (n_src_bitspersample != 16) {

	}
}

int main()
{
	//if (!load_wave(&wav, "1.wav")) {
	//	printf("Failed to load wave sound!\n");
	//	return 1;
	//}
	//free_wave(&wav);

	int src_samplerate = 44100;
	int dst_samplerate = 8000;

	int samplestep = src_samplerate / dst_samplerate;
	int sample_remainder = (src_samplerate % dst_samplerate);
	printf(
		"src samplerate: %d\n"
		"dst samplerate: %d\n"
		"sample step: %d\n"
		"sample remainder: %d\n",
		src_samplerate, dst_samplerate, samplestep, sample_remainder
	);

	return 0;
}