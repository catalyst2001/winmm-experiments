#include <Windows.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <intrin.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <CommCtrl.h>
#pragma comment(lib, "ComCtl32.lib")

#define WINDOW_CLASS "WindowClass"

HINSTANCE g_instance;
HWND h_wnd;
HWND h_trackbar_freq;
HWND h_volume;
HWND h_right_volume;

#define NUM_OF_BUFFERS 4

WAVEFORMATEX format;
HWAVEOUT h_waveout;
HANDLE h_event;
DWORD dw_buffer_size;
WAVEHDR buffers[NUM_OF_BUFFERS];

#define size(x) (sizeof(x) / sizeof(x[0]))

#define M_PI 3.1415926535
#define D2R (0.01745329251) //3.1415926535 / 180.0

float frequency = 0.001;
float gain;

void ErrorMessage(const char *p_format, ...);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void clampf(float *p_flt, float min, float max)
{
	if ((*p_flt) < min) {
		*p_flt = min;
		return;
	}

	if ((*p_flt) > max) {
		*p_flt = max;
		return;
	}
}

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

double time_get_seconds()
{
	LARGE_INTEGER time, freq;
	QueryPerformanceCounter(&time);
	QueryPerformanceFrequency(&freq);
	return (double)((double)time.QuadPart / (double)freq.QuadPart);
}

DWORD WINAPI audio_thread(LPVOID lpThreadParameter)
{
	float t = 0.f;
	size_t j = 0;
	PWAVEHDR p_buffer;
	float max_range = (float)(powf(2.f, (float)format.wBitsPerSample) / 2.f) - 1.0;
	while (true) {
		WaitForSingleObject(h_event, INFINITE);
		const size_t num_of_buffers = size(buffers);
		for (size_t i = 0; i < num_of_buffers; i++) {
			p_buffer = &buffers[i];
			if (p_buffer->dwFlags & MHDR_DONE) {
				p_buffer->dwFlags &= ~MHDR_DONE;
				short *p_samples = (short *)p_buffer->lpData;
				size_t num_samples = dw_buffer_size / format.nBlockAlign;
				static float alfa = 0.f;
				for (j = 0; j < num_samples; ) {
					float sample = sinf(alfa) / M_PI;
					p_samples[j++] = (short)(sample * gain * max_range); //L
					p_samples[j++] = (short)(sample * gain * max_range); //R
					alfa += frequency;
				}
				waveOutWrite(h_waveout, p_buffer, sizeof(*p_buffer));
			}
		}
	}
	return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEXA wcex;
	memset(&wcex, 0, sizeof(wcex));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcex.lpszClassName = WINDOW_CLASS;
	if (!RegisterClassExA(&wcex)) {
		ErrorMessage("Couldn't create window class");
		return 1;
	}

	g_instance = hInstance;
	INT width = 300;
	INT height = 300;
	INT xpos = (GetSystemMetrics(SM_CXSCREEN) / 2) - (width / 2);
	INT ypos = (GetSystemMetrics(SM_CYSCREEN) / 2) - (height / 2);
	h_wnd = CreateWindowExA(0, wcex.lpszClassName, "Генератор частоты", WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX, xpos, ypos, width, height, NULL, NULL, hInstance, NULL);
	if (!h_wnd) {
		ErrorMessage("Couldn't create window");
		return 2;
	}

	RECT rect;
	GetClientRect(h_wnd, &rect);
	rect.left += 10;
	rect.top += 10;
	rect.right -= 20;
	rect.bottom -= 20;
	CreateWindowExA(0, "static", "Частота", WS_VISIBLE | WS_CHILD, rect.left, rect.top, rect.right, 20, h_wnd, (HMENU)0, 0, 0);
	rect.top += 20 + 2;
	h_trackbar_freq = CreateWindowExA(0, TRACKBAR_CLASSA, "", WS_VISIBLE|WS_CHILD|TBS_ENABLESELRANGE, rect.left, rect.top, rect.right, 30, h_wnd, (HMENU)0, NULL, NULL);
	rect.top += 30 + 20;
	SendMessageA(h_trackbar_freq, TBM_SETRANGEMIN, FALSE, 1);
	SendMessageA(h_trackbar_freq, TBM_SETRANGEMAX, FALSE, 500);
	SendMessageA(h_trackbar_freq, TBM_SETPOS, TRUE, 0);

	frequency = (float)SendMessageA(h_trackbar_freq, TBM_GETPOS, 0, 0) * 0.001;

	CreateWindowExA(0, "static", "Громкость", WS_VISIBLE | WS_CHILD, rect.left, rect.top, rect.right, 20, h_wnd, (HMENU)0, 0, 0);
	rect.top += 20 + 2;
	h_volume = CreateWindowExA(0, TRACKBAR_CLASSA, "", WS_VISIBLE | WS_CHILD | TBS_ENABLESELRANGE, rect.left, rect.top, rect.right, 30, h_wnd, (HMENU)0, NULL, NULL);
	rect.top += 30 + 5;
	SendMessageA(h_volume, TBM_SETRANGEMIN, FALSE, 1);
	SendMessageA(h_volume, TBM_SETRANGEMAX, FALSE, 100);
	SendMessageA(h_volume, TBM_SETPOS, TRUE, 50);
	gain = (float)SendMessageA(h_volume, TBM_GETPOS, 0, 0) * 0.01;
	clampf(&gain, 0.f, 1.f);

	// --- init winmm ---
	CHAR desterr[512];
	MMRESULT result;
	format.cbSize = sizeof WAVEFORMATEX;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 1;
	format.wBitsPerSample = 16;
	format.nSamplesPerSec = 44100;
	format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
	format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;
	if ((result = waveOutOpen(&h_waveout, WAVE_MAPPER, &format, (DWORD_PTR)waveout_callback, (DWORD_PTR)NULL, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR) {
		waveOutGetErrorTextA(result, desterr, sizeof(desterr));
		MessageBoxA(0, desterr, "Failed to initialize wimm", MB_OK | MB_ICONERROR);
		return 1;
	}

	dw_buffer_size = format.nAvgBytesPerSec / 32;

	h_event = CreateEventA(NULL, FALSE, TRUE, NULL);
	if (!h_event) {
		DWORD error = GetLastError();
		sprintf_s(desterr, sizeof(desterr), "Error %d (0x%x)", error, error);
		MessageBoxA(0, "Failed to create event", desterr, MB_OK | MB_ICONERROR);
		return 1;
	}

	PWAVEHDR p_curr_hdr;
	for (size_t i = 0; i < size(buffers); i++) {
		p_curr_hdr = &buffers[i];
		memset(p_curr_hdr, 0, sizeof(WAVEHDR));
		p_curr_hdr->dwBufferLength = dw_buffer_size;
		p_curr_hdr->lpData = (LPSTR)calloc(p_curr_hdr->dwBufferLength, 1);
		assert(p_curr_hdr->lpData);
		waveOutPrepareHeader(h_waveout, p_curr_hdr, sizeof(WAVEHDR));
		p_curr_hdr->dwFlags |= MHDR_DONE;
	}
	waveOutRestart(h_waveout);
	HANDLE h_thread = CreateThread(NULL, NULL, audio_thread, NULL, NULL, NULL);
	ShowWindow(h_wnd, nCmdShow);
	UpdateWindow(h_wnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
    }
	TerminateThread(h_thread, 0);
    return (int) msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
		case WM_COMMAND: {
			int wmId = LOWORD(wParam);
			break;
		}

		case WM_HSCROLL:
		{
			frequency = (float)SendMessageA(h_trackbar_freq, TBM_GETPOS, 0, 0) * 0.0001;
			clampf(&frequency, 0.f, 1.f);
			gain = (float)SendMessageA(h_volume, TBM_GETPOS, 0, 0) * 0.01;
			clampf(&gain, 0.f, 1.f);
			return 0;
		}

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			break;
		}

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void ErrorMessage(const char *p_format, ...)
{
	char buffer[512];
	va_list argptr;
	va_start(argptr, p_format);
	vsprintf_s(buffer, sizeof(buffer), p_format, argptr);
	va_end(argptr);

	MessageBoxA(0, buffer, "Error", MB_OK | MB_ICONERROR);
}