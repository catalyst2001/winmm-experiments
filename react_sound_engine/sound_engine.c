#include "sound_engine.h"

// data for HSOUND
typedef struct snd_sound_s {
	FILE *fp;
	int flags;
	size_t block_size;
	size_t buffer_size;
	snd_uchar *p_buffer;
} snd_sound_t;