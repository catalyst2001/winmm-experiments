// Include driver interface realizations
#include "sound_engine_null.h"
#include "sound_engine_winmm.h"
#include "sound_engine_dsound.h"

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"

#ifdef LINUX
#include "sound_engine_alsa.h"
#endif

#define MAX_SOUND_BUFFERS 8
#define MAX_SOURCE_LIST_SIZE 2048

#define SND_CNT(x) (sizeof(x) / sizeof(x[0]))

typedef struct snd_sources_list_s {
	snd_source_t **p_sources;
	size_t capacity;
	size_t size;
} snd_sources_list_t;

/* globals */
int driver_index = -1;
int global_flags = 0;
int last_error_code = SNDE_ERROR_NONE;
snd_driver_interface_t *p_driver = NULL;
snd_format_t format;
snd_buffer_t snd_in_buffers[MAX_SOUND_BUFFERS];
snd_buffer_t snd_out_buffers[MAX_SOUND_BUFFERS];
malloc_func_t pfn_malloc = malloc;
calloc_func_t pfn_calloc = calloc;
realloc_func_t pfn_realloc = realloc;
pthread_t h_mixer_thread;
pthread_mutex_t h_synch_mutex;
snd_sources_list_t active_sources_list;

bool snd_source_list_alloc(snd_sources_list_t *p_dstlist, size_t start_size)
{
	p_dstlist->capacity = start_size;
	p_dstlist->size = 0;
	return (bool)(p_dstlist->p_sources = (snd_source_t **)pfn_malloc(p_dstlist->capacity * sizeof(snd_source_t *)));
}

void snd_source_list_free(snd_sources_list_t *p_dstlist)
{
	if (p_dstlist->p_sources)
		free(p_dstlist->p_sources);
}

void snd_source_list_add(snd_sources_list_t *p_dstlist, snd_source_t *p_source)
{
	if (p_dstlist->size >= p_dstlist->capacity) {
		printf("r1snde: Source list overflowed!\n");
		return;
	}
	p_dstlist->p_sources[p_dstlist->size++] = p_source;
	p_source->index_in_list = p_dstlist->size;
}

void snd_source_list_remove(snd_sources_list_t *p_dstlist, snd_source_t *p_source)
{
	if (p_source->index_in_list < p_dstlist->capacity) {
		if (p_source == p_dstlist->p_sources[p_source->index_in_list]) {

			// if element in end of list
			if (p_source->index_in_list == p_dstlist->size) {
				p_dstlist->size--; //decrease size of list
				return;
			}

			//move the element from the end to this position
			p_dstlist->p_sources[p_source->index_in_list] = p_dstlist->p_sources[p_dstlist->size];
			p_dstlist->size--; //decrease size of list
		}
		//else {
		//	// find source in list
		//	for (size_t i = 0; i < p_dstlist->size; i++) {
		//		if (p_dstlist->p_sources[i] == p_source) {

		//		}
		//	}
		//}
	}
}

#define snd_sources_get_source(sources_list, index) (&(sources_list)->p_sources[index])

// data for HSOUND
typedef struct snd_sound_s {
	FILE *fp;
	int flags;
	size_t block_size;
	size_t buffer_size;
	snd_uchar *p_buffer;
} snd_sound_t;

void snd_fill_format(snd_format_t *p_dst_format, long number_of_channels, long sample_rate, long bitrate)
{
	p_dst_format->num_of_channels = number_of_channels;
	p_dst_format->sample_rate = sample_rate;
	p_dst_format->bitrate = bitrate;
	p_dst_format->block_align = p_dst_format->num_of_channels * (p_dst_format->bitrate / 8);
	p_dst_format->bytes_per_sec = p_dst_format->block_align * p_dst_format->sample_rate;
}

bool snd_buffer_alloc(snd_buffer_t *p_buffer, int sample_rate, int num_of_channels)
{
	p_buffer->buffer_size = sample_rate * num_of_channels;
	p_buffer->samples_set = num_of_channels;
	return (bool)(p_buffer->p_data = (float *)pfn_calloc(p_buffer->buffer_size, sizeof(float)));
}

#define snd_set_last_error(error) last_error_code = error;
#define snd_get_last_error() (last_error_code)
#define snd_set_gflag(flag) global_flags |= flag
#define snd_set_gflags(flag) global_flags = flag
#define snd_get_gflags() (global_flags)
#define snd_unset_gflag(flag)\
	if (global_flags & flag)\
		global_flags &= ~flag;

/* direct interface */
bool init_fn(const snd_engine_initdata_t *p_init_data);
void shutdown_fn();

void set_flags_fn(int flags);
int  get_flags_fn();
const char *last_error_string_fn();
bool switch_driver_fn(int device_driver);
bool change_device_fn(int device_type, int device_id);

process_samples_fn set_listening_samples_callback_fn(process_samples_fn pfn_listening_samples_callback);

/* sounds */
HSOUND sound_load_ex_fn(const void *p_data, size_t size, int flags);
bool sound_free_ex_fn(HSOUND h_sound);

/* sound sources */
bool source_reset_fn(snd_source_t *p_source);
bool source_play_fn(snd_source_t *p_source, HSOUND h_sound);
void source_pause_fn(snd_source_t *p_source, HSOUND h_sound);
void source_stop_fn(snd_source_t *p_source, HSOUND h_sound);

void source_set_position_fn(snd_source_t *p_source, float position);
void source_set_speed_fn(snd_source_t *p_source, float position);
void source_set_pitch_fn(snd_source_t *p_source, float pitch);

/* sound effects */
bool fx_create_ex_fn(HFX *p_dst_hfx, size_t reserve, const char *p_fxname);
void *fx_get_data_fn(HFX src_hfx);
void fx_set_int_fn(HFX src_hfx, int offset, int ivalue);
int  fx_get_int_fn(HFX src_hfx, int offset);
void fx_set_float_fn(HFX src_hfx, int offset, float fvalue);
float fx_get_float_fn(HFX src_hfx, int offset);
void fx_set_vector_fn(HFX src_hfx, int offset, const float *p_fvvalue);
const float *fx_get_vector_fn(HFX src_hfx, int offset);
void fx_free_ex_fn(HFX src_hfx);

/* apply FX to source */
void source_apply_fx_fn(snd_source_t *p_source, HFX src_hfx);
void source_remove_fx_fn(snd_source_t *p_source, HFX src_hfx);

/* sound buffer */
bool buffer_create_fn(snd_buffer_t **p_dst_buffer, size_t num_of_samples, size_t num_of_buffers);
void buffer_free_fn(snd_buffer_t *p_src_buffer);

snd_engine_dt_t *get_sound_engine_api(int api_version)
{
	static const snd_engine_dt_t sndapi = {
		.init = init_fn,
		.shutdown = shutdown_fn,

		.set_flags = set_flags_fn,
		.get_flags = get_flags_fn,
		.last_error_string = last_error_string_fn,
		.switch_driver = switch_driver_fn,
		.change_device = change_device_fn,

		.set_listening_samples_callback = set_listening_samples_callback_fn,

		.sound_load_ex = sound_load_ex_fn,
		.sound_free_ex = sound_free_ex_fn,

		.source_reset = source_reset_fn,
		.source_play = source_play_fn,
		.source_pause = source_pause_fn,
		.source_stop = source_stop_fn,

		.source_set_position = source_set_position_fn,
		.source_set_speed = source_set_speed_fn,
		.source_set_pitch = source_set_pitch_fn,

		.fx_create_ex = fx_create_ex_fn,
		.fx_get_data = fx_get_data_fn,
		.fx_set_int = fx_set_int_fn,
		.fx_get_int = fx_get_int_fn,
		.fx_set_float = fx_set_float_fn,
		.fx_get_float = fx_get_float_fn,
		.fx_set_vector = fx_set_vector_fn,
		.fx_get_vector = fx_get_vector_fn,
		.fx_free_ex = fx_free_ex_fn,

		.source_apply_fx = source_apply_fx_fn,
		.source_remove_fx = source_remove_fx_fn,

		.buffer_create = buffer_create_fn,
		.buffer_free = buffer_free_fn
	};
	return &sndapi;
}

bool snd_get_driver_dt(snd_driver_interface_t **p_dst_driver, int driver_index)
{
	if (driver_index < DEVICE_NULL_DRIVER || driver_index >= MAX_DEVICES) {
		snd_set_last_error(SNDE_ERROR_DEVICE_DRIVER_NOT_SUPPORTED_BY_THIS_PLATFORM);
		return false;
	}

	static const snd_driver_interface_t *p_interfaces[] = {
		&impl_null_dt,
		&impl_winmm_dt,
		&impl_dsound_dt,
#ifdef LINUX
		&impl_alsa_dt;
#endif
	};
	(*p_dst_driver) = &p_interfaces[driver_index];
	return true;
}

void *mixer_thread_proc(void *p_arg)
{
	size_t index = 0;
	snd_buffer_t *p_buffer;
	snd_source_t *p_source;
	while ((snd_get_gflags() & SEF_MIXER_READY)) {
		for (size_t buffidx = 0; buffidx < SND_CNT(snd_out_buffers); buffidx++) {
			float samples_summ[2];
			p_buffer = &snd_out_buffers[buffidx];
			for (size_t srcidx = 0; srcidx < active_sources_list.size; srcidx++) {
				if ((p_source = snd_sources_get_source(&active_sources_list, srcidx))) {

				}
			}
		}
	}
	return NULL;
}

/* direct interface */
bool init_fn(const snd_engine_initdata_t *p_init_data)
{
	// check for already initialized
	if ((snd_get_gflags() & SEF_INITIALIZED) || p_driver) {
		snd_set_last_error(SNDE_ERROR_ALREADY_INITIALIZED);
		return false;
	}

	snd_fill_format(&format, p_init_data->num_of_channels, p_init_data->sample_rate, p_init_data->bitrate);

	/* check errors */
	if (!snd_get_driver_dt(&p_driver, p_init_data->driver))
		return false;

	if (!p_driver->driver_format_is_supported(&format)) {
		snd_set_last_error(SNDE_ERROR_DEVICE_NOT_SUPPORT_FORMAT);
		return false;
	}
	snd_set_gflag(SEF_INITIALIZED);

	/* init device */
	if (!p_driver->driver_init(&format)) {
		snd_set_last_error(SNDE_ERROR_DEVICE_DRIVER_REPORTED_ERROR);
		return false;
	}
	snd_set_gflag(SEF_DRIVER_READY);

	size_t i;
	snd_buffer_t *p_buffer;

	/* alloc in buffers */
	const size_t num_of_out_buffers = SND_CNT(snd_out_buffers);
	for (i = 0; i < num_of_out_buffers; i++) {
		if (!snd_buffer_alloc(&snd_out_buffers[i], format.sample_rate / num_of_out_buffers, format.num_of_channels)) {
			snd_set_last_error(SNDE_ERROR_OUT_OF_MEMORY);
			return false;
		}
	}

	/* alloc out buffers */
	const size_t num_of_in_buffers = SND_CNT(snd_in_buffers);
	for (i = 0; i < num_of_in_buffers; i++) {
		if (!snd_buffer_alloc(&snd_in_buffers[i], format.sample_rate / num_of_in_buffers, format.num_of_channels)) {
			snd_set_last_error(SNDE_ERROR_OUT_OF_MEMORY);
			return false;
		}
	}

	/* init active sources list */
	if (!snd_source_list_alloc(&active_sources_list, MAX_SOURCE_LIST_SIZE)) {
		snd_set_last_error(SNDE_ERROR_OUT_OF_MEMORY);
		return false;
	}

	/* init mixer */
	snd_set_gflag(SEF_MIXER_READY);
	if (pthread_create(&h_mixer_thread, (const pthread_attr_t *)NULL, mixer_thread_proc, 0)) {
		snd_set_last_error(SNDE_ERROR_CREATE_MIXER_THREAD);
		return false;
	}


	return true;
}

void shutdown_fn()
{
	if ((snd_get_gflags() & SEF_INITIALIZED) || p_driver) {
		snd_set_gflag(0);
		p_driver->driver_shutdown();
		p_driver = NULL;
	}
}

void set_flags_fn(int flags)
{
	snd_set_gflags(flags);
}

int get_flags_fn()
{
	snd_get_gflags();
}

const char *last_error_string_fn()
{
}

bool switch_driver_fn(int device_driver)
{
	snd_driver_interface_t *p_new_driver, *p_old_driver;
	// if selected driver interface not equal already used interface
	if (driver_index != device_driver) {
		driver_index = device_driver;

		// get old driver interface and new device interface
		if (!snd_get_driver_dt(&p_old_driver, driver_index) || snd_get_driver_dt(&p_new_driver, device_driver))
			return false;

		// init new interface
		if (!p_new_driver->driver_init(&format))
			return false;

		p_old_driver->driver_shutdown(); // shutdown old interface
		p_driver = p_new_driver; // store new device interface address
		return true;
	}
	return false;
}

bool change_device_fn(int device_type, int device_id)
{
	if (p_driver)
		return p_driver->driver_switch_device(device_type, device_id);
	
	return false;
}


process_samples_fn set_listening_samples_callback_fn(process_samples_fn pfn_listening_samples_callback)
{
}


/* sounds */
HSOUND sound_load_ex_fn(const void *p_data, size_t size, int flags)
{
}

bool sound_free_ex_fn(HSOUND h_sound)
{
}


/* sound sources */
bool source_reset_fn(snd_source_t *p_source)
{
}

bool source_play_fn(snd_source_t *p_source, HSOUND h_sound)
{
}

void source_pause_fn(snd_source_t *p_source, HSOUND h_sound)
{
}

void source_stop_fn(snd_source_t *p_source, HSOUND h_sound)
{
}


void source_set_position_fn(snd_source_t *p_source, float position)
{
}

void source_set_speed_fn(snd_source_t *p_source, float position)
{
}

void source_set_pitch_fn(snd_source_t *p_source, float pitch)
{
}


/* sound effects */
bool fx_create_ex_fn(HFX *p_dst_hfx, size_t reserve, const char *p_fxname)
{
}

void *fx_get_data_fn(HFX src_hfx)
{
}

void fx_set_int_fn(HFX src_hfx, int offset, int ivalue)
{
}

int  fx_get_int_fn(HFX src_hfx, int offset)
{
}

void fx_set_float_fn(HFX src_hfx, int offset, float fvalue)
{
}

float fx_get_float_fn(HFX src_hfx, int offset)
{
}

void fx_set_vector_fn(HFX src_hfx, int offset, const float *p_fvvalue)
{

}

const float *fx_get_vector_fn(HFX src_hfx, int offset)
{

}

void fx_free_ex_fn(HFX src_hfx)
{
	free(src_hfx);
}


/* apply FX to source */
void source_apply_fx_fn(snd_source_t *p_source, HFX src_hfx)
{

}

void source_remove_fx_fn(snd_source_t *p_source, HFX src_hfx)
{

}

/* sound buffer */
bool buffer_create_fn(snd_buffer_t **p_dst_buffer, size_t num_of_samples, size_t num_of_buffers)
{

}

void buffer_free_fn(snd_buffer_t *p_src_buffer)
{

}