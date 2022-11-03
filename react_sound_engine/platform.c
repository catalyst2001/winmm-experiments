#include "platform.h"

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, start_routine p_start_routine, void *arg)
{
	*thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)p_start_routine, arg, 0, NULL);
	return (*thread) == NULL;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
	if (WaitForSingleObject((HANDLE)thread, INFINITE) == WAIT_FAILED)
		return sys_last_error();

	return 0;
}

// 
// Mutex
// 
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	InitializeCriticalSection(&mutex->cs);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection(&mutex->cs);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	EnterCriticalSection(&mutex->cs);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	LeaveCriticalSection(&mutex->cs);
	return 0;
}

int access(const char *path, int amode)
{
	DWORD dw_flags = GetFileAttributesA(path);
	switch (amode) {
	case F_OK:
		return !((dw_flags != INVALID_FILE_ATTRIBUTES) && !(dw_flags & FILE_ATTRIBUTE_DIRECTORY));

	default:
		assert(0); //unimplemented
	}
	return 1;
}

void pthread_exit(void *retval)
{
	TerminateThread(GetCurrentThread(), 0);
}

int pthread_kill(pthread_t thread, int sig)
{
	switch (sig) {
	case SIGSTOP:
		SuspendThread((HANDLE)thread);
		break;

	case SIGCONT:
		ResumeThread((HANDLE)thread);
		break;

	default:
		assert(0); //unhandled SIGNAL
	}
	return 0;
}

int pthread_cancel(pthread_t target_thread)
{
	assert(0); //TODO: UNIMPLEMENTED
	return 0;
}

int sys_last_error()
{
	return GetLastError();
}

bool sys_file_exists(const char *p_path)
{
	DWORD dw_flags = GetFileAttributesA(p_path);
	return ((dw_flags != INVALID_FILE_ATTRIBUTES) && !(dw_flags & FILE_ATTRIBUTE_DIRECTORY));
}

double sys_get_seconds()
{
	LARGE_INTEGER time;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&time);
	return time.QuadPart / (double)frequency.QuadPart;
}
