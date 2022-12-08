#include "sound_engine_winmm.h"
#include <mmreg.h>

//typedef MMRESULT(WINAPI *PWAVEOPENFN)(
//	_Out_opt_ LPHWAVEIN phwi,
//	_In_ UINT uDeviceID,
//	_In_ LPCWAVEFORMATEX pwfx,
//	_In_opt_ DWORD_PTR dwCallback,
//	_In_opt_ DWORD_PTR dwInstance,
//	_In_ DWORD fdwOpen
//	);
//
//typedef UINT (WINAPI *WAVEGETNUMDEVSFN)(void);
//
//typedef MMRESULT (WINAPI *WAVEGETERRORTEXTA)(
//	_In_ MMRESULT mmrError,
//	_Out_writes_(cchText) LPSTR pszText,
//	_In_ UINT cchText
//);
//
//typedef MMRESULT (WINAPI *WAVEGETDEVCAPSA)(
//	_In_ UINT_PTR uDeviceID,
//	_Out_writes_bytes_(cbwic) LPWAVEINCAPSA pwic,
//	_In_ UINT cbwic
//);
//
//HMODULE h_winmm;
//union {
//	PWAVEOPENFN wave_open_vfn[2];
//	struct {
//		PWAVEOPENFN wave_in_open_pfn;
//		PWAVEOPENFN wave_out_open_pfn;
//	};
//} open;
//
//union {
//	WAVEGETDEVCAPSA wave_get_dev_capsa_vfn[2];
//	struct {
//		WAVEGETDEVCAPSA wave_in_get_dev_capsa_pfn;
//		WAVEGETDEVCAPSA wave_out_get_dev_capsa_pfn;
//	};
//} devcaps;
//
//union {
//	WAVEGETNUMDEVSFN wave_get_num_devs_vfn[2];
//	struct {
//		WAVEGETNUMDEVSFN wave_in_get_num_devs;
//		WAVEGETNUMDEVSFN wave_out_get_num_devs;
//	};
//} numdevs;
//
//#define wave_in_open open.wave_in_open_pfn
//#define wave_out_open open.wave_out_open_pfn
//#define wave_in_get_dev_capsa devcaps.wave_in_get_dev_capsa_pfn
//#define wave_out_get_dev_capsa devcaps.wave_out_get_dev_capsa_pfn
//#define wave_in_get_num_devs numdevs.wave_in_get_num_devs
//#define wave_out_get_num_devs numdevs.wave_out_get_num_devs
//
//bool init_windows_multimedia_library()
//{
//	HMODULE h_winmm = LoadLibraryA("winmm.dll");
//	if (!h_winmm)
//		return false;
//
//	wave_in_open = (PWAVEOPENFN)GetProcAddress(h_winmm, "waveInOpen");
//	wave_out_open = (PWAVEOPENFN)GetProcAddress(h_winmm, "waveOutOpen");
//	wave_in_get_dev_capsa = (WAVEGETDEVCAPSA)GetProcAddress(h_winmm, "waveInGetDevCapsA");
//	wave_out_get_dev_capsa = (WAVEGETDEVCAPSA)GetProcAddress(h_winmm, "waveOutGetDevCapsA");
//	wave_in_get_num_devs = (WAVEGETNUMDEVSFN)GetProcAddress(h_winmm, "waveInGetNumDevs");
//	wave_out_get_num_devs = (WAVEGETNUMDEVSFN)GetProcAddress(h_winmm, "waveOutGetNumDevs");
//}

HWAVEIN h_wavein;
HWAVEOUT h_waveout;
HANDLE h_event;
CRITICAL_SECTION cs;

bool winmm_driver_query_devices_number(int *p_devsnum, int device_type);
bool winmm_driver_query_device_information(snd_device_info_t *p_dst_info, int device_type, int device_index);
bool winmm_driver_format_is_supported(int device, snd_format_t *p_format);
bool winmm_driver_init(int device, snd_format_t *p_format);
bool winmm_driver_switch_device(int device_type, int device_index);
void winmm_driver_shutdown();
void winmm_driver_lock();
void winmm_driver_unlock();
void winmm_driver_wait();
bool winmm_driver_send_data(const snd_buffer_t *p_src_buffer);
void winmm_driver_set_listen_samples_callback(process_samples_fn listen_samples_processing_pfn);

process_samples_fn winmm_listen_samples_cb;
snd_driver_interface_t impl_winmm_dt = IDDI_INIT_FUNCTIONS(0, "winmm", winmm_driver_query_devices_number, winmm_driver_query_device_information, winmm_driver_format_is_supported, winmm_driver_init, winmm_driver_switch_device, winmm_driver_shutdown, winmm_driver_lock, winmm_driver_unlock, winmm_driver_wait, winmm_driver_send_data, winmm_driver_set_listen_samples_callback);

inline void snd_format_to_waveformatex(LPWAVEFORMATEX pdstwaveformatex, snd_format_t *p_srcformat)
{
	pdstwaveformatex->cbSize = sizeof(WAVEFORMATEX);
	pdstwaveformatex->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	pdstwaveformatex->nChannels = p_srcformat->num_of_channels;
	pdstwaveformatex->wBitsPerSample = p_srcformat->bitrate;
	pdstwaveformatex->nSamplesPerSec = p_srcformat->sample_rate;
	pdstwaveformatex->nBlockAlign = p_srcformat->block_align;
	pdstwaveformatex->nAvgBytesPerSec = p_srcformat->bytes_per_sec;
}

bool winmm_driver_query_devices_number(int *p_devsnum, int device_type)
{
	switch (device_type) {
	case IN_DEVICE:
		*p_devsnum = (int)waveInGetNumDevs();
		return true;

	case OUT_DEVICE:
		*p_devsnum = (int)waveOutGetNumDevs();
		return true;
	}
	return false;
}

#ifdef _MSC_VER
#define strcpy(dst, src)            strcpy_s(dst, sizeof(dst), src)
#define sprintf(dst, format, ...)   sprintf_s(dst, sizeof(dst), format, __VA_ARGS__)
#endif

bool winmm_driver_query_device_information(snd_device_info_t *p_dst_info, int device_type, int device_index)
{
	switch (device_type) {
		case IN_DEVICE: {
			WAVEINCAPSA indevinf;
			sizeof(indevinf);
			if (waveInGetDevCapsA((UINT_PTR)device_index, &indevinf, sizeof(indevinf)) != MMSYSERR_NOERROR)
				return false;

			strcpy(p_dst_info->device_name, indevinf.szPname);
			sprintf(p_dst_info->driver_version, "%d.%d", LOBYTE(indevinf.vDriverVersion), HIBYTE(indevinf.vDriverVersion));
			return true;
		}

		case OUT_DEVICE: {
			WAVEOUTCAPSA outdevinf;
			sizeof(outdevinf);
			if (waveOutGetDevCapsA((UINT_PTR)device_index, &outdevinf, sizeof(outdevinf)) != MMSYSERR_NOERROR)
				return false;

			strcpy(p_dst_info->device_name, outdevinf.szPname);
			sprintf(p_dst_info->driver_version, "%d.%d", LOBYTE(outdevinf.vDriverVersion), HIBYTE(outdevinf.vDriverVersion));
			return true;
		}
	}
	return false;
}

bool winmm_driver_format_is_supported(int device, snd_format_t *p_format)
{
	CHAR errbuffer[512];
	MMRESULT status;
	WAVEFORMATEX format;
	snd_format_to_waveformatex(&format, p_format);
	switch (device) {
		case OUT_DEVICE: {
			if ((status = waveOutOpen(0, (UINT)WAVE_MAPPER, &format, 0, 0, WAVE_FORMAT_QUERY)) != MMSYSERR_NOERROR) {
				waveOutGetErrorTextA(status, errbuffer, sizeof(errbuffer));
				printf("winmm_driver_format_is_supported: %s\n", errbuffer);
				return false;
			}
			return true;
		}

		case IN_DEVICE: {
			if ((status = waveInOpen(0, (UINT)WAVE_MAPPER, &format, 0, 0, WAVE_FORMAT_QUERY)) != MMSYSERR_NOERROR) {
				waveInGetErrorTextA(status, errbuffer, sizeof(errbuffer));
				printf("winmm_driver_format_is_supported: %s\n", errbuffer);
				return false;
			}
			return true;
		}
	}
	printf("winmm_driver_format_is_supported: Unknown device type!\n");
	return false;
}

void CALLBACK waveout_callback(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	UNREFERENCED_PARAMETER(dwInstance);
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

bool winmm_driver_init(int device, snd_format_t *p_format)
{
	MMRESULT status;
	WAVEFORMATEX format;
	CHAR errbuffer[512];
	snd_format_to_waveformatex(&format, p_format);
	switch (device) {
	case OUT_DEVICE: {
		if ((status = waveOutOpen(0, (UINT)WAVE_MAPPER, &format, 0, 0, WAVE_FORMAT_QUERY)) != MMSYSERR_NOERROR) {
			waveOutGetErrorTextA(status, errbuffer, sizeof(errbuffer));
			printf("winmm_driver_format_is_supported: %s\n", errbuffer);
			return false;
		}
		return true;
	}

	case IN_DEVICE: {
		if ((status = waveInOpen(0, (UINT)WAVE_MAPPER, &format, 0, 0, WAVE_FORMAT_QUERY)) != MMSYSERR_NOERROR) {
			waveInGetErrorTextA(status, errbuffer, sizeof(errbuffer));
			printf("winmm_driver_format_is_supported: %s\n", errbuffer);
			return false;
		}
		return true;
	}
	}
	return true;
}

bool winmm_driver_switch_device(int device_type, int device_index)
{
	return true;
}

void winmm_driver_shutdown()
{
}

void winmm_driver_lock()
{
}

void winmm_driver_unlock()
{
}

void winmm_driver_wait()
{
}

bool winmm_driver_send_data(const snd_buffer_t *p_src_buffer)
{
	return true;
}

void winmm_driver_set_listen_samples_callback(process_samples_fn listen_samples_processing_pfn)
{
	winmm_listen_samples_cb = listen_samples_processing_pfn;
}