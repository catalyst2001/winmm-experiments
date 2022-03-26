// 
// PCM WAV file recording demo
// Author: Deryabin K.
// Date: 26.03.2022
// 
// WARNING! Not use this implementation in real projects!
// The disadvantage of the demo program is that it does not use events
// to signal the readiness of reproducible buffers and always checks in the
// main thread for the presence of the set ready flag, which noticeably loads the core.
// 
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <assert.h> //check errors
#include <mmsystem.h> //waveInOpen etc
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

#define COUNT(x) (sizeof(x) / sizeof(x[0]))

#define RIFF_CHUNK  MAKEFOURCC('R', 'I', 'F', 'F')
#define FORMAT_WAVE MAKEFOURCC('W', 'A', 'V', 'E')
#define FMT_CHUNK   MAKEFOURCC('f', 'm', 't', ' ')
#define DATA_CHUNK  MAKEFOURCC('d', 'a', 't', 'a')

// riff chunk structure
//#pragma pack(push, 1)
typedef struct riff_s {
	int name;
	int size;
} riff_t;
//#pragma pack(pop)

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
// wav_riff_write
// 
// Writes riff chunk to file
// Return true if chunk succesfully writed to file
// otherwise false
// 
inline bool wav_riff_write(FILE *fp, UINT chunk_name, DWORD size)
{
	riff_t riff;
	riff.name = chunk_name;
	riff.size = size;
	return fwrite(&riff, sizeof(riff), 1, fp) == 1;
}

HWAVEIN h_wavein;
DWORD   bytes_per_second;
WAVEHDR buffers[8];

int main()
{
	SetConsoleTitleA("PCM WAV file recording demo");

	// fill record wave format
	WAVEFORMATEX record_format = {
		.cbSize = sizeof(WAVEFORMATEX),
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = 44100,
		.wBitsPerSample = 16
	};
	record_format.nBlockAlign = record_format.nChannels * (record_format.wBitsPerSample / 8);
	bytes_per_second = record_format.nAvgBytesPerSec = record_format.nBlockAlign * record_format.nSamplesPerSec;

	MMRESULT result;
	CHAR msgbuf[512];
	if ((result = waveInOpen(&h_wavein, WAVE_MAPPER, &record_format, CALLBACK_NULL, NULL, NULL)) != MMSYSERR_NOERROR) {
		waveInGetErrorTextA(result, msgbuf, sizeof(msgbuf));
		printf("waveInOpen failed: Error: '%s'\n", msgbuf);
		return 1;
	}

	// prepare record buffers
	DWORD i;
	WAVEHDR *p_current_buffer;
	const DWORD number_of_buffers = COUNT(buffers);
	for (i = 0; i < number_of_buffers; i++) {
		p_current_buffer = &buffers[i];
		memset(p_current_buffer, NULL, sizeof(*p_current_buffer));
		p_current_buffer->dwBufferLength = bytes_per_second;
		p_current_buffer->lpData = (char *)calloc(p_current_buffer->dwBufferLength, sizeof(char));
		assert(p_current_buffer->lpData);
		waveInPrepareHeader(h_wavein, p_current_buffer, sizeof(*p_current_buffer));
		waveInAddBuffer(h_wavein, p_current_buffer, sizeof(*p_current_buffer));
	}

	// wait for fill all buffers
	//for (int i = 0; i < number_of_buffers; i++)
	//	while (!(buffers[i].dwFlags & MHDR_DONE));

	DWORD recorded_seconds = 0;
	const DWORD record_seconds = 5;

	printf("Start recording file\n\n\n");
	FILE *fp = fopen("record.wav", "wb");
	if (!fp) {
		printf("Failed to open file!\n");
		return 2;
	}

	//fourcc name, size and 4 bytes identifier 'WAVE'
	const int riff_chunk_size = (sizeof(riff_t) + sizeof(int)); 

	// fourcc name, size and size WAVEFORMATEX struct (without DWORD cbSize filed)
	// that's why I subtract sizeof DWORD
	const int fmt_chunk_size = (sizeof(riff_t) + (sizeof(WAVEFORMATEX) - sizeof(WORD)));

	// add up all the previous chunk sizes and add the chunk header 'data'
	// as a result, we got the position in the file, starting from which we can record sound data :)
	const long data_chunk_offset = riff_chunk_size + fmt_chunk_size;

	// move to this position in the file to start writing data further
	fseek(fp, data_chunk_offset + sizeof(riff_t), SEEK_SET);

	// !! We will not create additional dynamic buffers to eventually copy the data from them to a file. !!
	// !! We will immediately write this data directly to a file. !!

	// begin record audio and write to file
	waveInStart(h_wavein); // start listening from microphone

	DWORD buffer_index = 0;
	DWORD total_record_bytes = 0;
	while (recorded_seconds < record_seconds) {
		p_current_buffer = &buffers[buffer_index];
		if (p_current_buffer->dwFlags & MHDR_DONE) {
			printf("\r%d/%d sec   percent: %1.f%%  total bytes: %d", recorded_seconds, record_seconds, (total_record_bytes / (float)(bytes_per_second * record_seconds) * 100.f), total_record_bytes);
			size_t writed_bytes = fwrite(p_current_buffer->lpData, 1, p_current_buffer->dwBytesRecorded, fp);
			if (writed_bytes != p_current_buffer->dwBytesRecorded) {
				assert(writed_bytes == p_current_buffer->dwBytesRecorded);
				printf("Can't write block data to file! Check disk free space!\n");
				fclose(fp);
				return 3;
			}
			total_record_bytes += p_current_buffer->dwBytesRecorded;
			waveInAddBuffer(h_wavein, p_current_buffer, sizeof(*p_current_buffer));
			buffer_index = (buffer_index + 1) % number_of_buffers; // cycling between buffers
			recorded_seconds++;
		}
	}
	waveInStop(h_wavein); // stop listening from microphone

	// unprepare record buffers
	for (i = 0; i < number_of_buffers; i++) {
		p_current_buffer = &buffers[i];
		waveInUnprepareHeader(h_wavein, p_current_buffer, sizeof(*p_current_buffer));
		if (p_current_buffer->lpData)
			free(p_current_buffer->lpData);
	}
	waveInClose(h_wavein);

	printf("\nTotal recorded bytes: %d\n", total_record_bytes);
	
	//save file size in bytes, for write this info to RIFF chunk
	long file_size = ftell(fp);
	
	//
	// Begin building file
	// Write WAV chunks
	// 
	fseek(fp, 0, SEEK_SET); // reset position in file to start

	// 
	// 'RIFF' chunk
	// 
	riff_t chunk;
	chunk.name = RIFF_CHUNK;
	chunk.size = file_size;
	if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) {
		printf("Failed to write 'RIFF' chunk header\n");
		fclose(fp);
		return 4;
	}

	UINT riff_format = FORMAT_WAVE;
	if (fwrite(&riff_format, sizeof(riff_format), 1, fp) != 1) {
		printf("Failed to write 'RIFF' chunk data\n");
		fclose(fp);
		return 5;
	}
	
	// 
	// 'fmt ' chunk
	// 
	chunk.name = FMT_CHUNK;
	chunk.size = (sizeof(WAVEFORMATEX) - sizeof(WORD));
	if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) {
		printf("Failed to write 'fmt ' chunk header\n");
		fclose(fp);
		return 6;
	}

	if (fwrite(&record_format, chunk.size, 1, fp) != 1) {
		printf("Failed to write 'fmt ' chunk data\n");
		fclose(fp);
		return 7;
	}

	// 
	// 'data' chunk
	// 
	chunk.name = DATA_CHUNK;
	chunk.size = file_size - data_chunk_offset;
	if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) {
		printf("Failed to write 'data' chunk header\n");
		fclose(fp);
		return 8;
	}

	// Initially, we started recording audio data, so we don't have to record it anymore.
	fclose(fp);
	return 0;
}