// 
// PCM WAV mixer demo
// Authors: Deryabin K. & Ilyichev I.
// Date: 29.03.2022
// 
// These are parts of the early development of the "Theta Sound Engine" for game engine.
// 
// 
// 
#include "../common/wav.h"
#include <mmsystem.h>
#include <assert.h>
#pragma comment(lib, "winmm.lib")

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

typedef intptr_t tse_device_id;

enum tse_device_init_status {
	TSE_DEVICE_INIT_STATUS_OK = 0,
	TSE_DEVICE_INIT_STATUS_NO_SUPPORT,
	TSE_DEVICE_INIT_FAILED
};

#define cnt(x) (sizeof(x) / sizeof(x[0]))

#define SHORT_MIN -32768
#define SHORT_MAX  32767
#define USHORT_MAX 65535

#define L 0
#define R 1
#define tse_clamp(x, mn, mx) ((x > mx) ? mx : ((x < mn) ? mn : x))

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
typedef struct sndsource_s {
	i16 flags;
	i16 status;
	ui8 source_type;
	ui64 read_position;
	sndformat_t format;
	ui64 buffer_size;
	i16 *p_samples;
	float position[3];
	float direction[3];
	float gain[2];
	const char *p_debugname;
} sndsource_t;

// Mixer structure
typedef struct sndmixer_s {
	HANDLE h_thread;
	HANDLE h_event;
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

	i32 number_of_sources;
	i32 active_sources;
	sndsource_t *p_snd_sources;

	f32 master_volume;
	i64 out_samples_count_per_buffer;
} sndmixer_t;

typedef struct snd_device_s {
	ui32 buffers_size;
	WAVEHDR buffers[8];
	HWAVEOUT h_waveout;
	sndmixer_t mixer;
	CRITICAL_SECTION cs;
} snd_device_t;

enum tse_status {
	TSE_OK,
	TSE_INVALID_PARAMETER,
	TSE_UNSUPPORTED_FORMAT
};

enum tse_sourcetype {
	TSE_SOURCE_3D = 0,	// for 3d
	TSE_SOURCE_FLAT		// for playing music in non-3D space
};

enum tse_sound_source_status {
	TSE_SSS_PLAYING = 1,// the source is playing now
	TSE_SSS_PAUSE,		// the source is paused
	TSE_SSS_STOPPED,	// thread stopped and can be restarted
	TSE_SSS_FREE,		// the source is free and can be used
};

#define TSE_SSF_LOOP (1 << 1)

// 
// tse_create_sound_source_ex
// 
// Creates an audio source for playback after mixing.
// The mixer accepts multiple audio sources and mixes them
// 
// Souce types:
//  
//  
int tse_create_sound_source_ex(const char *p_debug_source_name, const ui8 c_soucre_type, const i16 init_status, const i16 flags, const sndmixer_t *p_mixer, sndsource_t *p_source, ui8 type, const sndformat_t *p_srcformat, const float *p_src_channels_volume, char *p_data, i64 buffer_size)
{
	memset(p_source, 0, sizeof(*p_source));
	p_source->p_debugname = p_debug_source_name;
	p_source->status = init_status;
	p_source->flags = flags;
	if (c_soucre_type != TSE_SOURCE_3D && c_soucre_type != TSE_SOURCE_FLAT)
		return TSE_INVALID_PARAMETER;

	p_source->source_type = c_soucre_type;

	// copy audio format to sound steam structure
	p_source->format = *p_srcformat;

	// check format
	if (p_source->format.sample_rate != p_mixer->mixer_format.sample_rate)
		return TSE_UNSUPPORTED_FORMAT; // invalid sample rate

	if (p_source->format.bits_per_sample != p_mixer->mixer_format.bits_per_sample)
		return TSE_UNSUPPORTED_FORMAT; // invalid bps

	if (p_source->format.number_of_channels != p_mixer->mixer_format.number_of_channels)
		return TSE_UNSUPPORTED_FORMAT; // invalid channels number

	// set channels volume
	p_source->gain[L] = tse_clamp(p_src_channels_volume[L], 0.f, 1.f); //left
	p_source->gain[R] = tse_clamp(p_src_channels_volume[R], 0.f, 1.f); //right

	// store buffer address and size
	p_source->p_samples = (i16 *)p_data;
	p_source->buffer_size = buffer_size / p_mixer->mixer_format.bytes_per_sample; // bytes to samples count ( mixer default use 16-bit samples )
	p_source->read_position = 0;
	return TSE_OK;
}

enum tse_mixer_status {
	TSE_MIXER_STATUS_OK = 0,
	TSE_MIXER_STATUS_RUNNING,
	TSE_MIXER_STATUS_COMPLETED,
	TSE_MIXER_STATUS_ERROR_CREATE_THREAD
};

// 
// get_samples
// 
// Get stereo samples from sound source in range -1.0 -- 1.0
// 
void get_samples(const sndmixer_t *p_mixer, f32 *p_samples, sndsource_t *p_source)
{
	// mono
	/*if (p_mixer->mixer_format.number_of_channels == 1) {
		float sample = p_source->p_samples[p_source->read_position];
		p_samples[L] = p_source->gain[L] * sample;
		p_samples[R] = p_source->gain[R] * sample;
	}

	// stereo
	else */if (p_mixer->mixer_format.number_of_channels == 2) {
		p_samples[L] = p_source->gain[L] * (f32)(p_source->p_samples[p_source->read_position] / (f32)SHORT_MAX);
		p_samples[R] = p_source->gain[R] * (f32)(p_source->p_samples[p_source->read_position + 1] / (f32)SHORT_MAX);
	}
	p_source->read_position += p_mixer->mixer_format.number_of_channels;

	// check the end of samples buffer
	if (p_source->read_position >= p_source->buffer_size) {
		p_source->read_position = 0;

		// sound is looped ?
		if (!(p_source->flags & TSE_SSF_LOOP))
			p_source->status = TSE_SSS_STOPPED;
	}
}

DWORD WINAPI mixer_thread(snd_device_t *p_device)
{
	sndsource_t *p_sound_source;
	sndmixer_t *p_mixer = &p_device->mixer;
	while (p_mixer->status == TSE_MIXER_STATUS_RUNNING) {
		WaitForSingleObject(p_mixer->h_event, INFINITE);

		// playback buffers
		const size_t num_of_buffers = cnt(p_device->buffers);
		for (register size_t buffer_index = 0; buffer_index < num_of_buffers; buffer_index++) {
			
			PWAVEHDR p_current_buffer = &p_device->buffers[buffer_index];
			if (p_current_buffer->dwFlags & MHDR_DONE) {
				p_current_buffer->dwFlags &= ~MHDR_DONE;
				i16 *p_dest_samples = (i16 *)p_current_buffer->lpData;

				EnterCriticalSection(&p_device->cs);
				size_t i = 0;
				while (i < p_mixer->out_samples_count_per_buffer) {
					f32 source_samples[2];
					f32 mixed_samples[2] = { 0.f, 0.f }; // L  R

					// Mix audio sources samples
					for (i32 j = 0; j < p_mixer->number_of_sources; j++) {
						p_sound_source = &p_mixer->p_snd_sources[j];

						// is source playing ?
						if (p_sound_source->status == TSE_SSS_PLAYING) {
							get_samples(p_mixer, source_samples, p_sound_source);
							mixed_samples[L] += source_samples[L];
							mixed_samples[R] += source_samples[R];
						}
					}

					// Limit the range of sum of samples to prevent clicks
					mixed_samples[L] = tse_clamp(mixed_samples[L], -1.0f, 1.0f);
					mixed_samples[R] = tse_clamp(mixed_samples[R], -1.0f, 1.0f);

					// Fill samples
					p_dest_samples[i++] = (i16)(p_mixer->master_volume * mixed_samples[L] * (f32)SHORT_MAX);
					p_dest_samples[i++] = (i16)(p_mixer->master_volume * mixed_samples[R] * (f32)SHORT_MAX);
				}
				LeaveCriticalSection(&p_device->cs);

				// send audio data to device driver
				waveOutWrite(p_device->h_waveout, p_current_buffer, sizeof(*p_current_buffer));
			}
		}
	}
	return 0;
}

// 
// tse_init_mixer
// 
// Creates a mixer and starts playing audio sources.
// An audio source is a loaded music file that can be played in parallel with other playing sounds.
// 
// return TSE_MIXER_STATUS_OK if inited succesfully
// 
int tse_init_mixer(snd_device_t *p_device, ui8 mixer_format_type, const sndformat_t *p_format, i32 number_of_sources, f32 master_volume)
{
	p_device->mixer.master_volume = master_volume;
	p_device->mixer.status = TSE_MIXER_STATUS_RUNNING;
	p_device->mixer.number_of_sources = number_of_sources;
	p_device->mixer.p_snd_sources = (sndsource_t *)calloc(p_device->mixer.number_of_sources, sizeof(sndsource_t));
	assert(p_device->mixer.p_snd_sources);

	p_device->mixer.mixer_format = *p_format;
	p_device->mixer.out_samples_count_per_buffer = p_device->mixer.mixer_format.total_bytes_per_sec / p_device->mixer.mixer_format.number_of_channels;

	p_device->mixer.h_event = CreateEventA(NULL, FALSE, TRUE, NULL);
	if ((p_device->mixer.h_thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)mixer_thread, p_device, CREATE_SUSPENDED, NULL)) == NULL)
		return TSE_MIXER_STATUS_ERROR_CREATE_THREAD;

	return TSE_MIXER_STATUS_OK;
}

void CALLBACK waveout_callback(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	sndmixer_t *p_mixer = (sndmixer_t *)dwInstance;
	switch (uMsg)
	{
	case WOM_OPEN:
		break;

	case WOM_DONE:
		SetEvent(p_mixer->h_event);
		break;

	case WOM_CLOSE:
		break;
	}
}

// 
// tse_init
// 
// Init sound engine
// Create audio buffers, init mixer
// 
int tse_init(snd_device_t *p_device, char *p_desterr, int maxlen, const tse_device_id dev_id, const sndformat_t *p_format, const ui32 buffers_size, f32 master_volume)
{
	i32 error;
	MMRESULT mm_status;
	WAVEFORMATEX audio_format;
	memset(&audio_format, 0, sizeof(audio_format));
	audio_format.wFormatTag = WAVE_FORMAT_PCM;
	audio_format.cbSize = sizeof(audio_format);
	audio_format.nChannels = p_format->number_of_channels;
	audio_format.wBitsPerSample = p_format->bits_per_sample;
	audio_format.nSamplesPerSec = p_format->sample_rate;
	audio_format.nAvgBytesPerSec = p_format->total_bytes_per_sec;
	audio_format.nBlockAlign = p_format->block_align;
	if (waveOutOpen(0, (UINT)dev_id, &audio_format, 0, 0, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
		return TSE_DEVICE_INIT_STATUS_NO_SUPPORT;

	InitializeCriticalSection(&p_device->cs); // critical section for synchronize access to 'sound sources'

	// Init mixer
	if ((error = tse_init_mixer(p_device, (ui8)audio_format.wFormatTag, p_format, 128, master_volume)) != TSE_MIXER_STATUS_OK) {
		sprintf_s(p_desterr, maxlen, "Failed to initialize mixer. Error %d (0x%x)", error, error);
		return error;
	}
	
	if ((mm_status = waveOutOpen(&p_device->h_waveout, WAVE_MAPPER, &audio_format, (DWORD_PTR)waveout_callback, (DWORD_PTR)&p_device->mixer, CALLBACK_FUNCTION))) {
		waveOutGetErrorTextA(mm_status, p_desterr, maxlen);
		return TSE_DEVICE_INIT_FAILED;
	}
	p_device->buffers_size = buffers_size;

	// Init audio buffers and prepare headers
	PWAVEHDR p_curr_buffer;
	for (DWORD i = 0; i < cnt(p_device->buffers); i++) {
		p_curr_buffer = &p_device->buffers[i];
		memset(p_curr_buffer, 0, sizeof(*p_curr_buffer));
		p_curr_buffer->dwBufferLength = p_device->buffers_size;
		p_curr_buffer->lpData = (char *)calloc(p_device->buffers_size, sizeof(char));
		assert(p_curr_buffer->lpData);
		waveOutPrepareHeader(p_device->h_waveout, p_curr_buffer, sizeof(*p_curr_buffer));
		p_curr_buffer->dwFlags |= MHDR_DONE;
	}
	//waveOutReset(p_device->h_waveout);

	// Start mixer thread
	ResumeThread(p_device->mixer.h_thread);
	return TSE_DEVICE_INIT_STATUS_OK;
}

void tse_update_sources(snd_device_t *p_device, float *p_listener_origin, float *p_listener_dir)
{
	
}

wave_t sounds[2];
snd_device_t device;

int main()
{
	sndformat_t format;
	format.sample_rate = 44100;
	format.bits_per_sample = 16;
	format.number_of_channels = 2;
	format.bytes_per_sample = format.bits_per_sample / 8;
	format.block_align = format.number_of_channels * format.bytes_per_sample;
	format.total_bytes_per_sec = format.sample_rate * format.block_align;

	char errorstr[512];
	if (tse_init(&device, errorstr, sizeof(errorstr), WAVE_MAPPER, &format, format.total_bytes_per_sec, 1.0f) != TSE_DEVICE_INIT_STATUS_OK) {
		printf("Error init device: %s\n", errorstr);
		return 2;
	}

	char filename[64];
	sndsource_t sources[2];
	sndformat_t source_format;
	for (size_t i = 0; i < 2; i++) {
		sprintf_s(filename, sizeof(filename), "%d.wav", i);
		if (!load_wave(&sounds[i], filename)) {
			printf("Failed to load wave sound!\n");
			return 1;
		}
		source_format.sample_rate = sounds[i].sample_rate;
		source_format.bits_per_sample = sounds[i].bits_per_samle;
		source_format.bytes_per_sample = sounds[i].bytes_per_sample;
		source_format.number_of_channels = sounds[i].number_of_channels;

		float volume[][2] = {
			{ 1.0f, 1.0f },
			{ 1.0f, 1.0f }
		};

		if (tse_create_sound_source_ex(filename, TSE_SOURCE_FLAT, TSE_SSS_PLAYING, TSE_SSF_LOOP, &device.mixer, &device.mixer.p_snd_sources[i], WAVE_FORMAT_PCM,
			&source_format,
			volume[i],
			sounds[i].p_samples_data,
			sounds[i].buffer_size) != TSE_OK) {
			printf("Failed to create sound steam %d!\n", i);
		}
		sources[i].status = TSE_SSS_PLAYING;
	}
	WaitForSingleObject(device.mixer.h_thread, INFINITE);
	free_wave(&sounds[0]);
	free_wave(&sounds[1]);
	return 0;
}