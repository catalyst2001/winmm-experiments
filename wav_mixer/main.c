// 
// PCM WAV mixer demo
// Author: Deryabin K.
// Date: 29.03.2022
// 
// These are parts of the early development of the "Theta Sound Engine" for game engine.
// 
#include "../common/wav.h"
#include <mmsystem.h>
#include <assert.h>

wave_t wav;

typedef struct sndformat_s {
	int sample_rate;
	int bits_per_sample;
	int bytes_per_sample;
	short number_of_channels;
	int block_align;
	int total_bytes_per_sec;
} sndformat_t;

typedef char i8;
typedef unsigned char ui8;
typedef short i16;
typedef unsigned short ui16;
typedef int i32;
typedef unsigned int ui32;
typedef long long i64;
typedef unsigned long long ui64;

typedef float f32;
typedef double f64;

// 
// Audio source structure
// 
// Stores sound source data such as playback data, pointers to buffers that store sample data.
// Since it is planned to use this for 3D space, such a field as src_origin is provided, which
// stores data on the position of the sound source in the world and in which direction the sound is directed.
// ( Currently added for future updates, and will not be used at this time )
// 
// channels_volume:
//  0 - Left Front
//  1 - Right Front
//  2 - Left Back (N/A)
//  3 - Right Back (N/A)
// 
typedef struct sndsteam_s {
	i16 status;
	ui8 source_type;
	ui64 total_samples; // included all channels samples
	ui64 current_sample;
	sndformat_t format;
	i64 buffer_size;
	i16 *p_samples;
	float position[3];
	float direction[3];
	float channels_volume[4];
} sndsteam_t;

// Mixer structure
typedef struct sndmixer_s {
	HANDLE h_thread;
	ui8 status;
	i16 flags;

	// Reference format that all sound sources must conform to!
	// 
	// If the source has any different format parameter,
	// the mixer will not be able to process it correctly, therefore,
	// if the format is incorrect, we must convert
	// all sources to the reference format, otherwise it will
	// give an error that the sound cannot be played back.
	sndformat_t mixer_format;

	i32 number_of_streams;
	i32 active_streams;
	sndsteam_t *p_snd_streams;
} sndmixer_t;

enum tse_status {
	TSE_OK,
	TSE_INVALID_PARAMETER,
	TSE_UNSUPPORTED_FORMAT
};

enum tse_sourcetype {
	TSE_SOURCE_3D = 0,	// for 3d
	TSE_SOURCE_FLAT		// for playing music in non-3D space
};

enum tse_sound_stream_status {
	TSE_SSS_PLAYING = 0,// the stream is playing now
	TSE_SSS_PAUSE,		// the stream is paused
	TSE_SSS_STOPPED,	// thread stopped and can be restarted
	TSE_SSS_FREE,		// the stream is free and can be used
};

// 
// tse_create_sound_stream_ex
// 
// Creates an audio stream for playback after mixing.
// The mixer accepts multiple audio streams and mixes them
// 
// Souce types:
//  
//  
int tse_create_sound_stream_ex(const ui8 c_soucre_type, const sndmixer_t *p_mixer, sndsteam_t *p_stream, ui8 type, const sndformat_t *p_srcformat, const float *p_src_channels_volume, char *p_data, i64 buffer_size)
{
	p_stream->status = TSE_SSS_STOPPED;
	if (c_soucre_type != TSE_SOURCE_3D && c_soucre_type != TSE_SOURCE_FLAT)
		return TSE_INVALID_PARAMETER;

	p_stream->source_type = c_soucre_type;

	// check format
	if (p_stream->format.sample_rate != p_mixer->mixer_format.sample_rate)
		return TSE_UNSUPPORTED_FORMAT; // invalid sample rate

	if (p_stream->format.bits_per_sample != p_mixer->mixer_format.bits_per_sample)
		return TSE_UNSUPPORTED_FORMAT; // invalid bps

	if (p_stream->format.number_of_channels != p_mixer->mixer_format.number_of_channels)
		return TSE_UNSUPPORTED_FORMAT; // invalid channels number

	// copy audio format to sound steam structure
	p_stream->format = *p_srcformat;

	// set channels volume
	p_stream->channels_volume[0] = p_src_channels_volume[0]; //left
	p_stream->channels_volume[1] = p_src_channels_volume[1]; //right

	// store buffer address and size
	p_stream->p_samples = (i16 *)p_data;
	p_stream->buffer_size = buffer_size;
	return TSE_OK;
}

enum tse_mixer_status {
	TSE_MIXER_STATUS_OK = 0,
	TSE_MIXER_STATUS_RUNNING,
	TSE_MIXER_STATUS_COMPLETED,
	TSE_MIXER_STATUS_ERROR_CREATE_THREAD
};

#define tse_clamp(x, mn, mx) ((x > mx) ? mx : ((x < mn) ? mn : x))

DWORD WINAPI mixer_thread(sndmixer_t *p_mixer)
{
	ui64 current_sample;
	f32 left_sample, right_sample;
	while (p_mixer->status == TSE_MIXER_STATUS_RUNNING) {
		for (size_t i = 0; i < p_mixer->number_of_streams; i++) {
			//left_sample = tse_clamp()
		}
	}
	return 0;
}

// 
// tse_init_mixer
// 
// Creates a mixer and starts playing audio streams.
// An audio stream is a loaded music file that can be played in parallel with other playing sounds.
// 
// 
int tse_init_mixer(sndmixer_t *p_mixer, ui8 mixer_format_type, const sndformat_t *p_format, i32 number_of_streams)
{
	p_mixer->status = TSE_MIXER_STATUS_RUNNING;
	p_mixer->number_of_streams = number_of_streams;
	p_mixer->p_snd_streams = (sndsteam_t *)calloc(p_mixer->number_of_streams, sizeof(sndsteam_t));
	assert(p_mixer->p_snd_streams);

	if ((p_mixer->h_thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)mixer_thread, NULL, NULL, NULL)) == NULL)
		return TSE_MIXER_STATUS_ERROR_CREATE_THREAD;

	return TSE_MIXER_STATUS_OK;
}

typedef struct snd_device_s {
	ui32 buffers_size;
	WAVEHDR buffers[8];
	HWAVEOUT h_waveout;
	sndmixer_t mixer;
} snd_device_t;

typedef intptr_t tse_device_id;

enum tse_device_init_status {
	TSE_DEVICE_INIT_STATUS_OK = 0,
	TSE_DEVICE_INIT_STATUS_NO_SUPPORT,
	TSE_DEVICE_INIT_FAILED
};

#define cnt(x) (sizeof(x) / sizeof(x[0]))

// 
// tse_init
// 
// Init sound engine
// Create audio buffers, init mixer
// 
int tse_init(snd_device_t *p_device, char *p_desterr, int maxlen, const tse_device_id dev_id, const sndformat_t *p_format, const ui32 buffers_size)
{
	i32 error;
	MMRESULT mm_status;
	WAVEFORMATEX audio_format;
	memset(&audio_format, NULL, sizeof(audio_format));
	audio_format.wFormatTag = WAVE_FORMAT_PCM;
	audio_format.cbSize = sizeof(audio_format);
	audio_format.nChannels = p_format->number_of_channels;
	audio_format.wBitsPerSample = p_format->bits_per_sample;
	audio_format.nSamplesPerSec = p_format->sample_rate;
	audio_format.nAvgBytesPerSec = p_format->total_bytes_per_sec;
	audio_format.nBlockAlign = p_format->block_align;
	if (waveOutOpen(NULL, (UINT)dev_id, &audio_format, NULL, NULL, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
		return TSE_DEVICE_INIT_STATUS_NO_SUPPORT;
	
	if ((mm_status = waveOutOpen(&p_device->h_waveout, WAVE_MAPPER, &audio_format, NULL, NULL, CALLBACK_NULL))) {
		waveOutGetErrorTextA(mm_status, p_desterr, maxlen);
		return TSE_DEVICE_INIT_FAILED;
	}
	p_device->buffers_size = buffers_size;

	// Init audio buffers and prepare headers
	PWAVEHDR p_curr_buffer;
	for (DWORD i = 0; i < cnt(p_device->buffers); i++) {
		p_curr_buffer = &p_device->buffers[i];
		memset(p_curr_buffer, NULL, sizeof(*p_curr_buffer));
		p_curr_buffer->dwBufferLength = p_device->buffers_size;
		p_curr_buffer->lpData = (char *)calloc(p_device->buffers_size, sizeof(char));
		assert(p_curr_buffer->lpData);
		waveOutPrepareHeader(p_device->h_waveout, p_curr_buffer, sizeof(*p_curr_buffer));
	}
	waveOutReset(p_device->h_waveout);

	// Init mixer
	if ((error = tse_init_mixer(&p_device->mixer, audio_format.wFormatTag, p_format, 128)) != TSE_MIXER_STATUS_OK) {
		sprintf_s(p_desterr, maxlen, "Failed to initialize mixer. Error %d (0x%x)", error, error);
		return error;
	}
	return TSE_DEVICE_INIT_STATUS_OK;
}

int main()
{
	if (!load_wave(&wav, "1.wav")) {
		printf("Failed to load wave sound!\n");
		return 1;
	}
	free_wave(&wav);
	return 0;
}