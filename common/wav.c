#include "wav.h"


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