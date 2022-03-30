#include <stdio.h>
#include <stdbool.h>
#include <windows.h>

#define RIFF_CHUNK  MAKEFOURCC('R', 'I', 'F', 'F')
#define FORMAT_WAVE MAKEFOURCC('W', 'A', 'V', 'E')
#define FMT_CHUNK   MAKEFOURCC('f', 'm', 't', ' ')
#define DATA_CHUNK  MAKEFOURCC('d', 'a', 't', 'a')

// riff chunk structure
#pragma pack(push, 1)
typedef struct riff_s {
	int name;
	int size;
} riff_t;
#pragma pack(pop)

// wav file structure
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

#define RIFF_CHUNK_SIZE   (sizeof(riff_t) + sizeof(int))
#define FMT_CHUNK_SIZE    (sizeof(riff_t) + (sizeof(WAVEFORMATEX) - sizeof(WORD)))
#define DATA_CHUNK_OFFSET (RIFF_CHUNK_SIZE + FMT_CHUNK_SIZE)

// 
// find_riff_chunk
// 
// Finding a riff chunk in a file to read wav data blocks
// 
// p_dest_riff - pointer to riff_t structure instance to store readed data
// fp          - pointer to opened file stream
// name        - FOURCC chunk 4 bytes name
// 
long find_riff_chunk(riff_t *p_dest_riff, FILE *fp, int name);

// 
// load_wave
// 
// Load wav file to memory
// 
// p_wav - pointer wav file struct
// p_filename - pointer to filename
// 
// return true if file loaded correctly, false if file audio data type is not PCM or can't open file
// 
bool load_wave(wave_t *p_wav, const char *p_filename);

// 
// free_wave
// 
// Unload wav from memory
// 
// p_wav - pointer wav file struct
// 
void free_wave(wave_t *p_wav);

// ---------------------------------------------------------------------------------------------
// WAV file writing
// 
// The order of function calls should be like this:
// 1. wave_create - creates a file on disk and prepares it for writing data. Indents the beginning of the file to store riff chunk headers there later.
// 2. wave_write_data - writes audio data to a file.
// 3. wave_prepare_headers - prepares riff chunk headers for recording and writes them to the beginning of the file.
// 4. wave_close - closes the file
// ---------------------------------------------------------------------------------------------
typedef struct wave_stream_s {
	unsigned long long total_bytes;
	WAVEFORMATEX format;
	FILE *fp;
} wave_stream_t;

// 
// wave_create
// 
// Creates wav file on disk
// return true if file created
// 
bool wave_create(wave_stream_t *p_wavestrm, const WAVEFORMATEX *p_wfex, const char *p_filename);

// 
// wave_write_data
// 
// Write audio data to file
// return false if not free space on disk or data size equals NULL
// 
bool wave_write_data(wave_stream_t *p_wavestrm, char *p_data, size_t size);

// 
// wave_prepare_headers
// 
// Prepares riff chunk headers for recording and writes them to the beginning of the file.
// PCM
// 
bool wave_prepare_headers(wave_stream_t *p_wavestrm);

// 
// wave_get_total_bytes
// 
// Get total bytes from recorded wav
// 
#define wave_get_total_bytes(x) ((long long)(x)->total_bytes)

// 
// wave_close
// 
// Close and save file
// 
void wave_close(wave_stream_t *p_wavestrm);