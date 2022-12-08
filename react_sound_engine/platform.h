#pragma once
#include <stdbool.h>
#include <assert.h>

#ifdef _MSC_VER

// 
// POSIX Threads api implementation for Windows
// Added: 06.09.2022
// 

#include <Windows.h>

//threads
typedef void *pthread_t;
typedef long pthread_attr_t;
typedef void *(*start_routine)(void *);

#define SIGSTOP 23
#define SIGCONT 25

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, start_routine p_start_routine, void *arg);
int pthread_join(pthread_t thread, void **value_ptr);
void pthread_exit(void *retval);
int pthread_kill(pthread_t thread, int sig);
int pthread_cancel(pthread_t target_thread);

//synch
typedef struct pthread_mutex_s {
	CRITICAL_SECTION cs; //windows critical section
} pthread_mutex_t;

typedef long pthread_mutexattr_t;
int pthread_mutex_init_device(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

// 
// unistd.h   Files api implement
// 
#define R_OK 4 //readable
#define W_OK 2 //writable
#define X_OK 1 //executable
#define F_OK 0 //exists

int access(const char *path, int amode);

#define FILE_EXISTS(path) (!(access(path, F_OK)))

#else

#endif

// 
// Common typedefs
// 
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;

// 
// Common macroses
// 
#define MKINT(l, h)              ((uint)(l | (h << 16)))
#define LOWINT(i)                ((ushort)(i))
#define HIGHINT(i)               ((ushort)(i >> 16))

#define CNT(x)                   (sizeof(x) / sizeof(x[0]))

#define IABS(x)                  ((x > 0) ? x : -x)
#define FABS(x)                  ((x > 0.f) ? x : -x)
#define MIN(a, b)                ((a < b) ? a : b)
#define MAX(a, b)                ((a > b) ? a : b)
#define CLAMPMIN(x, minval)      ((x < minval) ? minval : x)
#define CLAMPMAX(x, maxval)      ((x > maxval) ? maxval : x)
#define CLAMP(x, minval, maxval) (((x < minval) ? minval : (x > maxval) ? maxval : x))
#define NOTHINGMACRO(x)

int sys_last_error();
bool sys_file_exists(const char *p_path);
double sys_get_seconds();