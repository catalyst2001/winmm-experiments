#include "sound_engine_null.h"

int driver_query_devices_number(int device);
bool driver_query_device_information(snd_device_info_t *p_dst_info, int device_type, int device_index);
bool driver_format_is_supported(snd_format_t *p_format);
bool driver_init(snd_format_t *p_format);
bool driver_switch_device(int device_type, int device_index);
void driver_shutdown();
void driver_lock();
void driver_unlock();
void driver_wait();
bool driver_send_data(const snd_buffer_t *p_src_buffer);
void driver_set_listen_samples_callback(process_samples_fn listen_samples_processing_pfn);

snd_driver_interface_t impl_null_dt = IDDI_INIT_FUNCTIONS(0, "NULL_DRIVER", driver_query_devices_number, driver_query_device_information, driver_format_is_supported, driver_init, driver_switch_device, driver_shutdown, driver_lock, driver_unlock, driver_wait, driver_send_data, driver_set_listen_samples_callback);

int driver_query_devices_number(int device)
{
	return 0;
}

bool driver_query_device_information(snd_device_info_t *p_dst_info, int device_type, int device_index)
{
	return true;
}

bool driver_format_is_supported(snd_format_t *p_format)
{
	return true;
}

bool driver_init(snd_format_t *p_format)
{
	return true;
}

bool driver_switch_device(int device_type, int device_index)
{
	return true;
}

void driver_shutdown()
{
}

void driver_lock()
{
}

void driver_unlock()
{
}

void driver_wait()
{
}

bool driver_send_data(const snd_buffer_t *p_src_buffer)
{
	return true;
}

void driver_set_listen_samples_callback(process_samples_fn listen_samples_processing_pfn)
{
}
