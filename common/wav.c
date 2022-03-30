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

bool wave_create(wave_stream_t *p_wavestrm, const WAVEFORMATEX *p_wfex, const char *p_filename)
{
	p_wavestrm->total_bytes = 0;
	p_wavestrm->fp = fopen(p_filename, "wb");
	if (!p_wavestrm->fp)
		return false;

	//fourcc name, size and 4 bytes identifier 'WAVE'
	const int riff_chunk_size = (sizeof(riff_t) + sizeof(int));

	// fourcc name, size and size WAVEFORMATEX struct (without DWORD cbSize filed)
	// that's why I subtract sizeof DWORD
	const int fmt_chunk_size = (sizeof(riff_t) + (sizeof(WAVEFORMATEX) - sizeof(WORD)));

	// add up all the previous chunk sizes and add the chunk header 'data'
	// as a result, we got the position in the file, starting from which we can record sound data :)
	const long data_chunk_offset = riff_chunk_size + fmt_chunk_size;

	// move to this position in the file to start writing data further
	fseek(p_wavestrm->fp, data_chunk_offset + sizeof(riff_t), SEEK_SET);
	return true;
}

bool wave_write_data(wave_stream_t *p_wavestrm, char *p_data, size_t size)
{
	if (!size)
		return false;

	if (fwrite(p_data, 1, size, p_wavestrm->fp) != size)
		return false;

	p_wavestrm->total_bytes += size;
	return true;
}

bool wave_prepare_headers(wave_stream_t *p_wavestrm)
{
	//save file size in bytes, for write this info to RIFF chunk
	long file_size = ftell(p_wavestrm->fp);

	//
	// Begin building file
	// Write WAV chunks
	// 
	fseek(p_wavestrm->fp, 0, SEEK_SET); // reset position in file to start

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

	if (fwrite(&p_wavestrm->format, chunk.size, 1, fp) != 1) {
		printf("Failed to write 'fmt ' chunk data\n");
		fclose(fp);
		return 7;
	}

	// 
	// 'data' chunk
	// 
	chunk.name = DATA_CHUNK;
	chunk.size = file_size - DATA_CHUNK_OFFSET;
	if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) {
		printf("Failed to write 'data' chunk header\n");
		fclose(fp);
		return 8;
	}
	return false;
}

void wave_close(wave_stream_t *p_wavestrm)
{
	fclose(p_wavestrm->fp);
}
