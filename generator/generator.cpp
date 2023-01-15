// 
// PCM WAV file loading and playing demo
// Author: Deryabin K.
// Date: 25.03.2022
// 
// WARNING! Not use this implementation in real projects!
// The disadvantage of the demo program is that it does not use events
// to signal the readiness of reproducible buffers and always checks in the
// main thread for the presence of the set ready flag, which noticeably loads the core.
// 
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <assert.h> //check errors
#include <mmsystem.h> //waveOutOpen etc
#include <mmreg.h>
#pragma comment(lib, "winmm.lib")

#include <stdio.h>
#include <stdbool.h>
#include <string.h> //memcpy

#include <math.h>

// debug print macro
#ifdef _DEBUG
#define DP(x, ...) printf(x, __VA_ARGS__)
#else
#define DP(x, ...) (void)0
#endif

void prepare_audio_buffers(HWAVEOUT h_waveout, PWAVEHDR p_buffers, DWORD number_of_buffers, DWORD buffer_size)
{
	PWAVEHDR p_buffer;
	for (DWORD i = 0; i < number_of_buffers; i++) {
		p_buffer = &p_buffers[i];
		memset(p_buffer, NULL, sizeof(WAVEHDR));
		p_buffer->dwBufferLength = buffer_size;
		p_buffer->lpData = (char *)calloc(p_buffer->dwBufferLength, sizeof(short));
		assert(p_buffer->lpData);
		waveOutPrepareHeader(h_waveout, p_buffer, sizeof(WAVEHDR));
		p_buffer->dwFlags |= MHDR_DONE;
	}
}

HWAVEOUT h_waveout;

WAVEHDR buffers[8];

#define COUNT(x) (sizeof(x) / sizeof(x[0]))


//https://habr.com/ru/post/425409/
/*
// *****************************************************************************
// ***   GenerateWave   ********************************************************
// *****************************************************************************
Result Application::GenerateWave(uint16_t* dac_data, uint32_t dac_data_cnt, uint8_t duty, WaveformType waveform)
{
  Result result;

  uint32_t max_val = (DAC_MAX_VAL * duty) / 100U;
  uint32_t shift = (DAC_MAX_VAL - max_val) / 2U;

  switch(waveform)
  {
	case WAVEFORM_SINE:
	  for(uint32_t i = 0U; i < dac_data_cnt; i++)
	  {
		dac_data[i] = (uint16_t)((sin((2.0F * i * PI) / (dac_data_cnt + 1)) + 1.0F) * max_val) >> 1U;
		dac_data[i] += shift;
	  }
	  break;

	case WAVEFORM_TRIANGLE:
	  for(uint32_t i = 0U; i < dac_data_cnt; i++)
	  {
		if(i <= dac_data_cnt / 2U)
		{
		  dac_data[i] = (max_val * i) / (dac_data_cnt / 2U);
		}
		else
		{
		  dac_data[i] = (max_val * (dac_data_cnt - i)) / (dac_data_cnt / 2U);
		}
		dac_data[i] += shift;
	  }
	  break;

	case WAVEFORM_SAWTOOTH:
	  for(uint32_t i = 0U; i < dac_data_cnt; i++)
	  {
		dac_data[i] = (max_val * i) / (dac_data_cnt - 1U);
		dac_data[i] += shift;
	  }
	  break;

	case WAVEFORM_SQUARE:
	  for(uint32_t i = 0U; i < dac_data_cnt; i++)
	  {
		dac_data[i] = (i < dac_data_cnt / 2U) ? max_val : 0x000;
		dac_data[i] += shift;
	  }
	  break;

	default:
	  result = Result::ERR_BAD_PARAMETER;
	  break;
  }

  return result;
}
*/

#define TOTAL_BYTES_PER_SEC(pf) ((pf)->nAvgBytesPerSec / ((pf)->wBitsPerSample / 8))
#define BUFFER_SIZE (pf, n) (TOTAL_BYTES_PER_SEC(pf) / n)

HANDLE h_event;
WAVEFORMATEX device_format;
const size_t number_of_buffers = COUNT(buffers);
size_t buffer_size;

void CALLBACK waveout_callback(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	switch (uMsg)
	{
	case WOM_OPEN:
		break;

	case WOM_DONE:
		SetEvent(h_event);
		break;

	case WOM_CLOSE:
		break;
	}
}

#define FLOAT_DSP

float clamp(float a, float m, float mx)
{
	if (a < m)
		return m;

	if (a > mx)
		return mx;

	return a;
}

DWORD WINAPI thread(LPVOID param)
{
	size_t i, j, k;
	float max_value = 10767.f;/*(powf(2.f, (float)device_format.wBitsPerSample) - 1.f) / 2.f;*/
	printf("%f\n", max_value);
	while (true) {
		WaitForSingleObject(h_event, INFINITE);
		//printf("WaitForSingleObject\n");
		for (i = 0; i < number_of_buffers; i++) {
			if (buffers[i].dwFlags & WHDR_DONE) {
				buffers[i].dwFlags &= ~WHDR_DONE;
				short *p_samples = (short *)buffers[i].lpData;
				for (size_t j = 0; j < buffer_size;) {
					short sample = (short)(sin(2.0F * j * 3.14f));
					for (size_t k = 0; k < device_format.nChannels; k++) {
						p_samples[j + k] = sample;
						//printf("sample: %d\n", p_samples[j + k]);
					} 

					j += device_format.nChannels;
				}
				waveOutWrite(h_waveout, &buffers[i], sizeof(buffers[i]));
			}
		}
	}
	return 0;
}

int main()
{
	SetConsoleTitleA("Waveform generator");

	// fill waveout device struct
	device_format.cbSize = sizeof(device_format);
	device_format.wFormatTag = WAVE_FORMAT_PCM;
	device_format.nSamplesPerSec = 44100;
	device_format.wBitsPerSample = 16;
	device_format.nChannels = 1;
	device_format.nBlockAlign = (device_format.wBitsPerSample / 8) * device_format.nChannels;
	device_format.nAvgBytesPerSec = device_format.nSamplesPerSec * device_format.nBlockAlign;

	h_event = CreateEventA(NULL, FALSE, TRUE, NULL);

	// opening out device
	MMRESULT result;
	CHAR msgbuf[512];
	if ((result = waveOutOpen(&h_waveout, WAVE_MAPPER, &device_format, (DWORD_PTR)waveout_callback, (DWORD_PTR)NULL, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR) {
		waveOutGetErrorTextA(result, msgbuf, sizeof(msgbuf));
		MessageBoxA(0, msgbuf, "waveOutOpen failed", 0);
		return 4;
	}

	// allocate and prepare audio buffers for writing
	buffer_size = (device_format.nAvgBytesPerSec / (device_format.wBitsPerSample / 8)) / number_of_buffers;
	printf("Audio buffers: %d  Buffer size: %d\n", number_of_buffers, buffer_size);
	prepare_audio_buffers(h_waveout, buffers, number_of_buffers, buffer_size);
	waveOutReset(h_waveout);
	HANDLE h_thread = CreateThread(0, 0, thread, 0, 0, 0);
	WaitForSingleObject(h_thread, INFINITE);

	// free audio buffers
	for (size_t i = 0; i < number_of_buffers; i++) {
		waveOutUnprepareHeader(h_waveout, &buffers[i], sizeof(buffers[i]));
		free(buffers[i].lpData);
	}
	waveOutClose(h_waveout); // close out device
	return 0;
}