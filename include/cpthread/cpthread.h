#ifndef CPTHREAD_H
#define CPTHREAD_H

#include "cplatforms.h"
#if defined PLATFORM_WINDOWS
#include <windows.h>
#define THREAD DWORD WINAPI
#elif defined PLATFORM_UNIX
#include <pthread.h> 
#include <unistd.h>
#define THREAD void*
#endif

//A Windows-Unix cross platform encapsulation of threads.
//Threads can be created, joined (waited for) or forcefully terminated
//A static sleep function sleeps the current thread for some milliseconds
class thread
{
#if defined PLATFORM_WINDOWS
private:
	HANDLE thr;

public:
	inline void create(void* func, void* data = NULL) { thr = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)func, data, NULL, NULL); }
	inline void join() { WaitForSingleObject(thr, INFINITE); }
	inline void terminate() { TerminateThread(thr, -1); }
	inline static void sleep(int millis) { Sleep(millis); }

#elif defined PLATFORM_UNIX
private:
	pthread_t thr;

public:
	inline void create(void* func(void*), void* data = NULL) { pthread_create(&thr, NULL, func, data); }
	inline void join() { pthread_join(thr, NULL); }
	inline void terminate() { pthread_cancel(thr); }
	inline static void sleep(int millis) { usleep(millis*1000); }
#endif
};

#endif
