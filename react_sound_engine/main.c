#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include "sound_engine.h"

snd_engine_dt_t *p_sndengine;

void error_callback(const char *p_text)
{
	printf("SOUND_ENGINE: %s\n", p_text);
}

/**
* print_devices
*
* Print devices information
*/
void print_devices(snd_engine_dt_t *p_engine)
{
	/* print IN devices */
	int i;
	int num_of_devs;
	snd_device_info_t devinf;
	if (p_engine->get_devices_number(&num_of_devs, IN_DEVICE)) {
		for (i = 0; i < num_of_devs; i++) {
			if (!p_engine->get_device_info(&devinf, IN_DEVICE, i)) {
				printf("Failed to get device info! Error: %s\n", p_engine->last_error_string());
				return 1;
			}

			/* print device info */
			printf(
				"-------- In devices information ------------\n"
				"Device name: %s\n"
				"Driver description: %s\n"
				"Supported formats ( samplerate: %d | bitrate: %d | channels: %d )\n\n",
				devinf.device_name,
				devinf.driver_version,
				devinf.format.sample_rate, devinf.format.bitrate, devinf.format.num_of_channels
			);
		}
	}

	/* print OUT devices */
	if (p_engine->get_devices_number(&num_of_devs, OUT_DEVICE)) {
		for (i = 0; i < num_of_devs; i++) {
			if (!p_engine->get_device_info(&devinf, OUT_DEVICE, i)) {
				printf("Failed to get device info! Error: %s\n", p_engine->last_error_string());
				return 1;
			}

			/* print device info */
			printf(
				"-------- Out devices information ------------\n"
				"Device name: %s\n"
				"Driver description: %s\n"
				"Supported formats ( samplerate: %d | bitrate: %d | channels: %d )\n\n",
				devinf.device_name,
				devinf.driver_version,
				devinf.format.sample_rate, devinf.format.bitrate, devinf.format.num_of_channels
			);
		}
	}
}

int main()
{
	//p_sndengine = get_sound_engine_api(SND_API_VERSION);
	//p_sndengine->set_info_message_callback(error_callback);

	//snd_engine_initdata_t init_props;
	//init_props.driver = DEVICE_NULL_DRIVER;
	//init_props.sample_rate = 44100;
	//init_props.bitrate = 16;
	//init_props.num_of_channels = 2;
	//if (!p_sndengine->init(&init_props)) {
	//	printf("Failed to init sound engine! Error: %s\n", p_sndengine->last_error_string());
	//	return 1;
	//}
	//print_devices(p_sndengine);

	//

	//HSOUND h_snd1;
	//if (!(h_snd1 = p_sndengine->sound_load_ex("1.wav", 0, SF_LOAD_FROM_DISK))) {
	//	printf("Failed to load sound! Error: %s\n", p_sndengine->last_error_string());
	//	return 1;
	//}

	//snd_source_t source1;
	//if (p_sndengine->source_create(&source1, h_snd1)) {
	//	p_sndengine->source_play(&source1, h_snd1);
	//}

	//p_sndengine->wait_threads();
	//p_sndengine->shutdown();

	size_t samples = 44100;
	size_t channels = 2;
	for (size_t sample_set = 0; sample_set < samples; sample_set++) {
		printf(" %d", sample_set * channels);
	}

	return 0;
}