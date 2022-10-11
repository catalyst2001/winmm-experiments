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
#define DEVICE_NULL_DRIVER 0
#define DEVICE_WINMM_DRIVER 1
#define DEVICE_DSOUND_DRIVER 2
#define DEVICE_ALSA_DRIVER 3

/* sound engine errors */
enum SNDE_ERRORS {
	SNDE_ERROR_NONE = 0,										  // Operation completed successfully without errors
	SNDE_ERROR_DEVICE_DRIVER_NOT_SUPPORTED_BY_THIS_PLATFORM,	  // The device driver interface not implemented for this platform
	SNDE_ERROR_DEVICE_DRIVER_REPORTED_ERROR,				      // The device driver reported an unrecoverable error
	SNDE_ERROR_DEVICE_DRIVER_SWITCHING,							  // The device driver is in the switching phase
	SNDE_ERROR_DEVICE_NOT_SUPPORT_FORMAT,						  // The device driver does not support the specified audio format
	SNDE_ERROR_DEVICE_SWITCH,
};

typedef void (*process_samples_fn)(float *p_samples, size_t num_of_samples_set, int samplerate, int num_of_channels);

/* sound flags */
#define SF_NONE      (0)	   // 
#define SF_STREAMING (1 << 0)  // 

typedef void *SNDHANDLE;
typedef SNDHANDLE HSOUND;

typedef struct snd_engine_initdata_s {
	int device_driver;
	int bit_rate;
	int sample_rate;
	int num_of_channels;
} snd_engine_initdata_t;

typedef struct snd_source_s {
	size_t current_sample_set; //
} snd_source_t;

typedef struct snd_engine_dt_s {
	/* direct interface */
	bool               (*init)(const snd_engine_initdata_t *p_init_data);
	bool               (*is_initialized)();
	void               (*shutdown)();


	const char        *(*last_error_string)();
	void               (*start_playing)();
	void               (*stop_playing)();
	bool               (*is_playing)();
	bool               (*switch_driver)(int device_driver);

	/* microphone interface */
	process_samples_fn (*set_listening_samples_callback)(process_samples_fn pfn_listening_samples_callback);
	bool               (*start_listening)();
	bool               (*stop_listening)();
	bool               (*is_listening)();

	/* sounds */
	HSOUND             (*sound_load)(const char *p_path, int flags);
	bool               (*sound_free)(HSOUND h_sound);

	/* sound sources */
	bool               (*source_reset)(snd_source_t *p_source);
	bool               (*source_play)(snd_source_t *p_source, HSOUND h_sound);
	void               (*source_pause)(snd_source_t *p_source, HSOUND h_sound);
	void               (*source_stop)(snd_source_t *p_source, HSOUND h_sound);

	void               (*source_set_position)(snd_source_t *p_source, float position);
	void               (*source_set_speed)(snd_source_t *p_source, float position);
	void               (*source_set_pitch)(snd_source_t *p_source, float position);



} snd_engine_dt_t;