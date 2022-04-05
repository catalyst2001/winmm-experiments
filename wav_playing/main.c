// 
// PCM WAV file loading and playing demo
// Author: Deryabin K.
// Date: 25.03.2022
// 
// WARNING! Not use this implementation in real projects!
// The disadvantage of the demo program is that it does not use events
// to signal the readiness of reproducible buffers and always checks in the
// main thread for the presence of the set ready flag, which noticeably loads the core.
// 
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <assert.h> //check errors
#include <mmsystem.h> //waveOutOpen etc
#pragma comment(lib, "winmm.lib")

#include <stdio.h>
#include <stdbool.h>
#include <string.h> //memcpy

// debug print macro
#ifdef _DEBUG
#define DP(x, ...) printf(x, __VA_ARGS__)
#else
#define DP(x, ...) (void)0
#endif

// riff chunk structure
#pragma pack(push, 1)
typedef struct riff_s {
	int name;
	int size;
} riff_t;
#pragma pack(pop)

typedef struct wave_s {
	int wave_format;
	int buffer_size;
	int sample_rate;
	int bits_per_samle;
	int bytes_per_sample;
	int number_of_channels;
	int block_align;
	int total_bytes_per_sec;
	char *p_samples_data;
} wave_t;

// 
// find_riff_chunk
// 
// Finding a riff chunk in a file to read wav data blocks
// 
// p_dest_riff - pointer to riff_t structure instance to store readed data
// fp          - pointer to opened file source
// name        - FOURCC chunk 4 bytes name
// 
long find_riff_chunk(riff_t *p_dest_riff, FILE *fp, int name)
{
	long offset = 0;
	rewind(fp);
	while (!feof(fp) && !ferror(fp)) {

		// read riff chunk
		if (fread(p_dest_riff, sizeof(riff_t), 1, fp) != 1)
			return 0;

		offset = ftell(fp); // record current offset in file
		if (p_dest_riff->name == name)
			return offset; // if riff chunk id equals - return current offset

		// move sizeof(riff_t) - 1 byte to the left to move 1 byte
		// in the file instead of the size of the riff_chunk structure
		fseek(fp, offset - (sizeof(riff_t) - 1), SEEK_SET);
	}
	return 0; // nothing found
}

#define RIFF_CHUNK  MAKEFOURCC('R', 'I', 'F', 'F')
#define FORMAT_WAVE MAKEFOURCC('W', 'A', 'V', 'E')
#define FMT_CHUNK   MAKEFOURCC('f', 'm', 't', ' ')
#define DATA_CHUNK  MAKEFOURCC('d', 'a', 't', 'a')

bool load_wave(wave_t *p_wav, const char *p_filename)
{
	FILE *fp = fopen(p_filename, "rb");
	if (!fp)
		return false;

	riff_t riff;

	// Read 'RIFF' chunk
	int id;
	if (!find_riff_chunk(&riff, fp, RIFF_CHUNK))
		return false; // 'RIFF' chunk not found

	DP("'RIFF' chunk size: %d bytes (%d kbytes)\n", riff.size, riff.size / 1000);

	// can't read format id
	if (fread(&id, sizeof(id), 1, fp) != 1)
		return false;

	// format is not wave
	if (id != FORMAT_WAVE)
		return false;


	// Read 'fmt ' chunk
	if (!find_riff_chunk(&riff, fp, FMT_CHUNK))
		return false; // 'fmt ' chunk not found

	DP("'fmt ' chunk size: %d bytes (%d kbytes)\n", riff.size, riff.size / 1000);

	WAVEFORMATEX wf;
	memset(&wf, NULL, sizeof(wf));
	if (fread(&wf, sizeof(wf) - sizeof(wf.cbSize), 1, fp) != 1)
		return false; // failed to read waveformat

	p_wav->wave_format = wf.wFormatTag;
	p_wav->sample_rate = wf.nSamplesPerSec;
	p_wav->bits_per_samle = wf.wBitsPerSample;
	p_wav->bytes_per_sample = (p_wav->bits_per_samle / 8);
	p_wav->number_of_channels = wf.nChannels;
	p_wav->block_align = (p_wav->number_of_channels * p_wav->bytes_per_sample);
	p_wav->total_bytes_per_sec = (p_wav->block_align * p_wav->sample_rate);

	// Read 'data' chunk
	if (!find_riff_chunk(&riff, fp, DATA_CHUNK))
		return false; // failed to find 'data' chunk

	DP("'data' chunk size: %d bytes (%d kbytes)\n", riff.size, riff.size / 1000);
	p_wav->buffer_size = riff.size;
	p_wav->p_samples_data = (char *)calloc(p_wav->buffer_size, 1); //allocate memory for samples

	// if allocation failed
	if (!p_wav->p_samples_data) {
		DP("Failed allocate %d bytes for samples!\n", p_wav->buffer_size);
		return false;
	}

	if (fread(p_wav->p_samples_data, 1, p_wav->buffer_size, fp) != p_wav->buffer_size) {
		DP("Failed to read samples data from file!\n");
		return false;
	}
	fclose(fp);
	return true;
}

void free_wave(wave_t *p_wav)
{
	// free allocated memory
	if (p_wav->p_samples_data)
		free(p_wav->p_samples_data);
}

MMRESULT audio_format_supported(LPWAVEFORMATEX p_waveformat, UINT devid)
{
	return (waveOutOpen(
		NULL, // ptr can be NULL for query 
		devid, // the device identifier 
		p_waveformat, // defines requested format 
		NULL, // no callback 
		NULL, // no instance data 
		WAVE_FORMAT_QUERY));  // query only, do not open device 
}

int prepare_audio_buffers(HWAVEOUT h_waveout, PWAVEHDR p_buffers, DWORD number_of_buffers, DWORD buffer_size)
{
	PWAVEHDR p_buffer;
	for (DWORD i = 0; i < number_of_buffers; i++) {
		p_buffer = &p_buffers[i];
		memset(p_buffer, NULL, sizeof(WAVEHDR));
		p_buffer->dwBufferLength = buffer_size;
		p_buffer->lpData = (char *)calloc(p_buffer->dwBufferLength, sizeof(char));
		assert(p_buffer->lpData);
		waveOutPrepareHeader(h_waveout, p_buffer, sizeof(WAVEHDR));
	}
}

HWAVEOUT h_waveout;

// 
// copy_samples
// 
// Copies sound sample data into audio buffers and returns true if the buffers have been filled and are ready to be played
// otherwise false
// 
bool copy_samples(PWAVEHDR p_buffer, DWORD buffer_size, DWORD *p_playingpos, const wave_t *p_wave, bool loop)
{
	p_buffer->dwFlags &= ~WHDR_DONE; // remove done flag
	DWORD remaining_size = buffer_size;
	if ((*p_playingpos + buffer_size) >= p_wave->buffer_size) {
		remaining_size = (p_wave->buffer_size - *p_playingpos);

		memcpy(p_buffer->lpData, &p_wave->p_samples_data[*p_playingpos], remaining_size);
		p_buffer->dwBufferLength = remaining_size;

		// loop this sound ?
		if (loop) {
			*p_playingpos = 0; // reset position in buffer
			return true; // continue playing
		}
		return false; // stop playing
	}

	// copy audio blocks from wave samples data to audio buffers until the position in samples buffer reaches the end
	memcpy(p_buffer->lpData, &p_wave->p_samples_data[*p_playingpos], remaining_size);
	p_buffer->dwBufferLength = remaining_size;
	*p_playingpos += remaining_size; // increment position in buffer
	return true;
}

WAVEHDR  buffers[8];
DWORD    max_audio_buffer_size;

#define COUNT(x) (sizeof(x) / sizeof(x[0]))

int main()
{
	SetConsoleTitleA("PCM WAV file playing demo");

	wave_t wav;
	printf("Loading wav file...\n");
	if (!load_wave(&wav, "aryx.wav")) {
		printf("Failed to load wav file\n");
		return 1;
	}

	// check audio format type
	if (wav.wave_format != WAVE_FORMAT_PCM) {
		assert(wav.wave_format == WAVE_FORMAT_PCM);
		printf("Loaded WAV have is not PCM audio format!  This demo support only PCM audio format!\n");
		return 2;
	}
	printf("wav loaded!\n\n\n");

	// fill waveout device struct
	WAVEFORMATEX device_format;
	device_format.cbSize = sizeof(device_format);
	device_format.wFormatTag = WAVE_FORMAT_PCM;
	device_format.nSamplesPerSec = wav.sample_rate;
	device_format.wBitsPerSample = wav.bits_per_samle;
	device_format.nBlockAlign = wav.block_align;
	device_format.nAvgBytesPerSec = wav.total_bytes_per_sec;
	device_format.nChannels = wav.number_of_channels;

	// check support device this format
	if (audio_format_supported(&device_format, WAVE_MAPPER) != MMSYSERR_NOERROR) {
		printf("Loaded format is not supported by device!\n");
		return 3;
	}

	// opening out device
	MMRESULT result;
	CHAR msgbuf[512];
	if ((result = waveOutOpen(&h_waveout, WAVE_MAPPER, &device_format, NULL, NULL, CALLBACK_NULL))) {
		waveOutGetErrorTextA(result, msgbuf, sizeof(msgbuf));
		printf("waveOutOpen failed. Error: %s\n", msgbuf);
		return 4;
	}
	
	// allocate and prepare audio buffers for writing
	const DWORD buffer_size = 1024;
	const DWORD number_of_buffers = COUNT(buffers);
	const bool  loop_track_playing = 1;

	DWORD playing_position = 0;
	prepare_audio_buffers(h_waveout, buffers, number_of_buffers, buffer_size);
	waveOutReset(h_waveout);

	DWORD i;
	for (i = 0; i < number_of_buffers; i++) {
		copy_samples(&buffers[i], buffer_size, &playing_position, &wav, loop_track_playing);
		waveOutWrite(h_waveout, &buffers[i], sizeof(buffers[i]));
	}

	// playing wav file
	while (true) {
		for (i = 0; i < number_of_buffers; i++) {
			if (buffers[i].dwFlags & WHDR_DONE) {
				if (!copy_samples(&buffers[i], buffer_size, &playing_position, &wav, loop_track_playing))
					goto __endplaying;

				// send audio buffer to device for playback
				waveOutWrite(h_waveout, &buffers[i], sizeof(buffers[i]));

				// update progress in console
				printf("\rpos %d/%d  track playing percent: %.1f%%        ", playing_position, wav.buffer_size, (playing_position / (float)wav.buffer_size) * 100.f);
			}
		}
	}

__endplaying:

	// free audio buffers
	for (i = 0; i < number_of_buffers; i++) {
		waveOutUnprepareHeader(h_waveout, &buffers[i], sizeof(buffers[i]));
		free(buffers[i].lpData);
	}
	waveOutClose(h_waveout); // close out device
	free_wave(&wav);
	return 0;
}