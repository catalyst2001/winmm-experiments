#include <Windows.h>
#include <dsound.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

extern "C" {
#include "../common/wav.h"
};
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")

WAVEFORMATEX format;
IDirectSound8 *p_directsound;
IDirectSoundBuffer8 *p_soundbuffer8;
DWORD buffer_length;
HANDLE notify_event;
DSBPOSITIONNOTIFY pos_notity[2];

wave_t wave_file;

#define CNT(a) (sizeof(a) / sizeof(a[0]))

double time_ms()
{
	LARGE_INTEGER freq, counter;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&counter);
	return counter.QuadPart / (double)freq.QuadPart;
}

/* WAVEFORM VIEW */
HDC h_dc;
HWND h_wnd;
RECT rect;

LRESULT CALLBACK waveform_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	case WM_SIZE:
		GetClientRect(hwnd, &rect);
		break;
	}
	return DefWindowProcA(hwnd, msg, wparam, lparam);
}

bool waveform_create()
{
	WNDCLASSA wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = "waveform_window";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = GetModuleHandleA(0);
	wc.lpfnWndProc = waveform_window_proc;

	h_wnd = CreateWindowExA(0, wc.lpszClassName, "waveform", WS_VISIBLE | WS_OVERLAPPEDWINDOW, 0, 0, 1000, 200, NULL, (HMENU)0, NULL, NULL);
	if (!h_wnd)
		return false;


}

void waveform_generate(short *p_dst, int n_samples, float frequency, float amplitude)
{
#define DEG2RAD(y) ((y) / 180.f)
	//static float time = 0.f;
	const float pi2 = 3.14 * 2;
	static float alpha = 0.f;
	for (int i = 0; i < n_samples; i++) {
		float sample = sinf(DEG2RAD(pi2 * alpha));
		//float sample = sinf(2 * pi2 * frequency * (float)time_ms() + 0.f);
		//printf("time=%f freq=%f freq_rad_s=%f data=%f\n", t, freq_hz, freq_rad_s, sample);

		if (sample < -1.f)
			sample = 1.f;
		if (sample > 1.f)
			sample = 1.f;

		p_dst[i] = (short)(sample * 32767);
		alpha += 0.2f;
	}
}

size_t position = 0; // position in wav samples buffer

void fill_buffer(char *p_data, int length)
{
	/* simple DSP implement */
	int i = 0, j = 0;
	float multiplier = (powf(2.f, (float)wave_file.bits_per_samle) / 2.f) - 1.f; // compute ranges
	short *p_dst_samples = (short *)p_data; // dest samples for send to device
	short *p_src_samples = (short *)&wave_file.p_samples_data[position]; // source samples

	if ((position + wave_file.bytes_per_sample) >= wave_file.buffer_size)
		position = 0; //reset position in buffer

	int num_samples = (length / wave_file.bytes_per_sample);
	while (i < num_samples) {

		/* convert samples to float */
		float src_samples[2], dst_samples[2];
		src_samples[0] = (float)(p_src_samples[j++] / multiplier);
		src_samples[1] = (float)(p_src_samples[j++] / multiplier);

		/* work with samples data */

		dst_samples[0] = src_samples[0] * -1;
		dst_samples[1] = src_samples[1] * -1;

		p_dst_samples[i++] = (short)(dst_samples[0] * multiplier);
		p_dst_samples[i++] = (short)(dst_samples[1] * multiplier);
	}
	position += (num_samples * wave_file.bytes_per_sample); //increment position in wav samples buffer
}

int main()
{
	if (FAILED(DirectSoundCreate8(0, &p_directsound, 0))) {
		printf("Failed to initialize direct sound 8\n");
		return 1;
	}

	if (FAILED(IDirectSound8_SetCooperativeLevel(p_directsound, GetForegroundWindow(), DSSCL_PRIORITY))) {
		printf("failed to set direct sound cooperative level\n");
		IDirectSound8_Release(p_directsound);
		return 1;
	}

	memset(&format, 0, sizeof(format));
	format.cbSize = sizeof(WAVEFORMATEX);
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = 44100;
	format.wBitsPerSample = 16;
	format.nBlockAlign = (format.wBitsPerSample / 8) * format.nChannels;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	buffer_length = format.nAvgBytesPerSec / 4; // buffer length is 1/4 second

	DSBUFFERDESC bufdesc;
	IDirectSoundBuffer *p_dsbuffer;
	memset(&bufdesc, 0, sizeof(bufdesc));
	bufdesc.dwSize = sizeof(bufdesc);
	bufdesc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;
	bufdesc.dwBufferBytes = buffer_length * 2; //for double buffering
	bufdesc.lpwfxFormat = &format;
	if (FAILED(IDirectSound8_CreateSoundBuffer(p_directsound, &bufdesc, &p_dsbuffer, NULL))) {
		printf("failed to create direct sound buffer!\n");
		IDirectSound8_Release(p_directsound);
		return 1;
	}
	IDirectSoundBuffer_QueryInterface(p_dsbuffer, IID_IDirectSoundBuffer8, (void **)&p_soundbuffer8);
	IDirectSoundBuffer_Release(p_dsbuffer);
	
	IDirectSoundNotify *p_dsnotify;
	IDirectSoundBuffer8_QueryInterface(p_soundbuffer8, IID_IDirectSoundNotify8, (void**)&p_dsnotify);

	// register buffer positions nofify
	notify_event = CreateEventA(0, 0, 0, 0);

	// first part of buffer
	pos_notity[0].hEventNotify = notify_event;
	pos_notity[0].dwOffset = buffer_length;

	// next part of buffer
	pos_notity[1].hEventNotify = notify_event;
	pos_notity[1].dwOffset = 0;
	IDirectSoundNotify_SetNotificationPositions(p_dsnotify, CNT(pos_notity), pos_notity);
	IDirectSoundBuffer8_Play(p_soundbuffer8, 0, 0, DSBPLAY_LOOPING);

	// loading wav file
	if (!load_wave(&wave_file, "../sounds/techno44100.wav")) {
		printf("Failed to load wav file!\n");
		return 1;
	}

	char *p_buffer;
	int buffer_index = 1;
	while (1) {
		DWORD size;
		if (WaitForSingleObject(notify_event, INFINITE) == WAIT_OBJECT_0) {
			IDirectSoundBuffer8_Lock(p_soundbuffer8, buffer_length * buffer_index, buffer_length, (void **)&p_buffer, &size, 0, 0, 0);
			fill_buffer(p_buffer, buffer_length);
			IDirectSoundBuffer8_Unlock(p_soundbuffer8, p_buffer, size, 0, 0);
		}
		buffer_index = (buffer_index + 1) % 2;
	}
	IDirectSoundNotify_Release(p_dsnotify);
	IDirectSoundBuffer8_Release(p_soundbuffer8);
	IDirectSound8_Release(p_directsound);
	free_wave(&wave_file);
	return 0;
}