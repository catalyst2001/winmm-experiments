/**
* React Sound Engine
* This is part of the "React Game Engine" source code
* 
* Version: 0.0.2
* Created: 01.10.2022
* Modified: 10.10.2022
* 
* Authors:
*  Kirill Deryabin (catalyst, skymotion, malloc) - Implementation of management interfaces. Implementation of algorithms. Lead coder.
*  Dr. David K.E. Green (dkgrdatasystems) - Algorithms and theory of digital sound. Mathematical calculations and transformations.
*  Ryan Gregg
*  Evgeny Krasnikov (Jin X) - Algorithms and theory of digital sound. Mathematical calculations and transformations.
*  Ilya Ilyichev (shar1otte) - Mathematical calculations and transformations
*  
*  
**/
#pragma once
#include <stdio.h>
#include <stdbool.h>

typedef unsigned char snd_uchar;

/* supported devices */
enum DEVICE_DRIVERS {
	DEVICE_NULL_DRIVER = 0,	 // Null device driver. Enable this for debug
	DEVICE_WINMM_DRIVER,	 // Windows Multimedia Library using
	DEVICE_DSOUND_DRIVER,	 // Windows Direct Sound
#ifdef LINUX
	DEVICE_ALSA_DRIVER,	 // Linux Alsa audio subsystem
#endif
	MAX_DEVICES
};

/* sound engine errors */
enum SNDE_ERRORS {
	SNDE_ERROR_NONE = 0,										  // Operation completed successfully without errors
	SNDE_ERROR_ALREADY_INITIALIZED,
	SNDE_ERROR_DEVICE_DRIVER_NOT_SUPPORTED_BY_THIS_PLATFORM,	  // The device driver interface not implemented for this platform
	SNDE_ERROR_DEVICE_DRIVER_REPORTED_ERROR,				      // The device driver reported an unrecoverable error
	SNDE_ERROR_DEVICE_DRIVER_SWITCHING,							  // The device driver is in the switching phase
	SNDE_ERROR_DEVICE_NOT_SUPPORT_FORMAT,						  // The device driver does not support the specified audio format
	SNDE_ERROR_DEVICE_CHANGE,                                     // Device in changing state
	SNDE_ERROR_OUT_OF_MEMORY,									  // Critical error. System out of memory
	SNDE_ERROR_CREATE_MIXER_THREAD,								  // Mixer thread is not created
};

typedef void (*process_samples_fn)(float *p_samples, size_t num_of_samples_set, int samplerate, int num_of_channels);

/* sound flags */
#define SF_NONE      (0)	   // For sound source not enabled flags
#define SF_STREAMING (1 << 0)  // Source enabled streaming processing
#define SF_LOAD_FROM_DISK (1 << 1)

/* sound source flags */
#define SSF_PITCH_CHANGED (1 << 0) // Sound source has changed pitch
#define SSF_SPEED_CHANGED (1 << 1) // Source playback speed changed
#define SSF_FX_ENABLED (1 << 2)	   // Source effect enabled
#define SSF_REALTIME (1 << 3)      // Source is the source currently receiving the signal
#define SSF_PLAYING (1 << 4)
#define SSF_LOOPED (1 << 5)

typedef void *SNDHANDLE;
typedef SNDHANDLE HSOUND;
typedef SNDHANDLE HFX;

#define SND_FORMAT_PCM 0
#define SND_FORMAT_FLOAT 1

typedef struct snd_format_s {
	long audio_format;
	long bitrate;
	long sample_rate;
	long num_of_channels;
	long block_align;
	long bytes_per_sec;
} snd_format_t;

typedef void *(*malloc_func_t)(size_t size);
typedef void *(*calloc_func_t)(size_t count, size_t size);
typedef void *(*realloc_func_t)(void *p_oldptr, size_t new_size);

typedef struct snd_engine_initdata_s {
	int driver;
	int bitrate;
	int sample_rate;
	int num_of_channels;
	malloc_func_t malloc_func;
	calloc_func_t calloc_func;
	realloc_func_t realloc_func;
} snd_engine_initdata_t;

typedef struct snd_buffer_s {
	size_t buffer_size;
	size_t samples_set;
	float *p_data;
} snd_buffer_t;

typedef struct snd_source_s {
	int flags;
	HSOUND h_sound;
	size_t index_in_list; // index in active sources list
	snd_buffer_t samples_buffer; //TODO: ???

	// sample set it is number of sample sets for each channel
	// that is: (0)[1 channel, 2 channel]  (1)[1 channel, 2 channel]  (2)[1 channel, 2 channel]
	// address formule:  offset = index * num_of_channels
	size_t current_sample_set;
	float speed;
	float pitch;
} snd_source_t;

/* sound engine global flags */
#define SEF_PLAYING (1 << 0)	  // Set this bit for start playing sounds
#define SEF_LISTENING (1 << 1)	  // Set this bit for listening data from microphone
#define SEF_MIXER_READY (1 << 2)  // Sound engine mixer has ready
#define SEF_INITIALIZED (1 << 3)  // Sound engine has initialized
#define SEF_DRIVER_READY (1 << 4)

#define SND_API_VERSION 1

/*
*  Device types
*  for devices queries and volume settings
*/
#define IN_DEVICE 0
#define OUT_DEVICE 1

/* for change_device function */
#define DEVICE_DEFAULT -1

#define MAX_DEVICE_INFO 64 // max length of device info string

/* basic device info structure */
typedef struct snd_device_info_s {
	char device_name[MAX_DEVICE_INFO];
	char driver_version[MAX_DEVICE_INFO];
	snd_format_t format;
} snd_device_info_t;

typedef struct snd_fx_s {
	int flags;
	size_t struct_size;
	void (*process_samples_fn)(float *p_samples, int number_of_channels);
} snd_fx_t;

typedef void (*info_msg_callback_pfn)(const char *p_text);

typedef struct snd_engine_dt_s {
	/* direct interface */
	void               (*set_info_message_callback)(info_msg_callback_pfn callback_pfn);
	bool               (*init)(const snd_engine_initdata_t *p_init_data);
	void               (*wait_threads)();
	void               (*shutdown)();

	void               (*set_flags)(int flags);
	int                (*get_flags)();
	const char        *(*last_error_string)();
	bool               (*get_devices_number)(int *p_devsnum, int device_type);
	bool               (*get_device_info)(snd_device_info_t *p_dstinfo, int device_type, int device_index);
	bool               (*switch_driver)(int device_driver);
	bool               (*change_device)(int device_type, int device_id);
	void               (*set_volume)(int device_type, float volume);
	float              (*get_volume)(int device_type);

	/* microphone interface */
	process_samples_fn (*set_listening_samples_callback)(process_samples_fn pfn_listening_samples_callback);

	/* sounds */
	HSOUND             (*sound_load_ex)(const void *p_data, size_t size, int flags);
	bool               (*sound_free_ex)(HSOUND h_sound);
	float              (*sound_get_duration)(HSOUND h_sound);

	/* sound sources */
	bool               (*source_create)(snd_source_t *p_dst_source, HSOUND h_sound);
	bool               (*source_reset)(snd_source_t *p_source);
	bool               (*source_play)(snd_source_t *p_source, HSOUND h_sound);
	void               (*source_pause)(snd_source_t *p_source, HSOUND h_sound);
	void               (*source_stop)(snd_source_t *p_source, HSOUND h_sound);

	void               (*source_set_position)(snd_source_t *p_source, float position);
	void               (*source_set_speed)(snd_source_t *p_source, float position);
	void               (*source_set_pitch)(snd_source_t *p_source, float pitch);

	/* sound effects */
	bool               (*fx_create_ex)(HFX *p_dst_hfx, size_t reserve, const char *p_fxname);
	void              *(*fx_get_data)(HFX *p_src_hfx);
	void               (*fx_set_int)(HFX *p_src_hfx, int offset, int ivalue);
	int                (*fx_get_int)(HFX *p_src_hfx, int offset);
	void               (*fx_set_float)(HFX *p_src_hfx, int offset, float fvalue);
	float              (*fx_get_float)(HFX *p_src_hfx, int offset);
	void               (*fx_set_vector)(HFX *p_src_hfx, int offset, const float *p_fvvalue);
	const float       *(*fx_get_vector)(HFX *p_src_hfx, int offset);
	void               (*fx_free_ex)(HFX *p_src_hfx);

	/* apply FX to source */
	void               (*source_apply_fx)(snd_source_t *p_source, HFX *p_src_hfx);
	void               (*source_remove_fx)(snd_source_t *p_source, HFX *p_src_hfx);
	void               (*source_set_order_fx)(snd_source_t *p_source, HFX *p_src_hfx, int order);

	/* sound buffer */
	bool               (*buffer_create)(snd_buffer_t **p_dst_buffer, size_t num_of_samples, size_t num_of_buffers);
	void               (*buffer_free)(snd_buffer_t *p_src_buffer);

	/* engine functions */
} snd_engine_dt_t;

snd_engine_dt_t *get_sound_engine_api(int api_version);

#define IDDI_INIT_FUNCTIONS(flags, driver_name, query_devices_number_fn, query_device_information_fn, format_is_supported, init, switch_device, shutdown, lock, unlock, wait, send_data, set_listen_samples_callback) \
	{ flags, driver_name, query_devices_number_fn, query_device_information_fn, format_is_supported, init, switch_device, shutdown, lock, unlock, wait, send_data, set_listen_samples_callback }

/*   Independend Device Driver Interface (IDDI)   */
typedef struct snd_driver_interface_s {
	int flags;
	const char *p_driver_name;
	int  (*driver_query_devices_number)(int device);
	bool (*driver_query_device_information)(snd_device_info_t *p_dst_info, int device_type, int device_index);

	bool (*driver_format_is_supported)(snd_format_t *p_format);
	bool (*driver_init)(snd_format_t *p_format);
	bool (*driver_switch_device)(int device_type, int device_index);
	void (*driver_shutdown)();
	void (*driver_lock)();
	void (*driver_unlock)();
	void (*driver_wait)();
	bool (*driver_send_data)(const snd_buffer_t *p_src_buffer);
	void (*driver_set_listen_samples_callback)(process_samples_fn listen_samples_processing_pfn);
} snd_driver_interface_t;