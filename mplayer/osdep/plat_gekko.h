#ifndef __PLAT_GEKKO_H__
#define __PLAT_GEKKO_H__

#ifdef GEKKO

#include <sys/time.h>
#include <sys/dir.h>
#include <unistd.h>

#include <gctypes.h>
#include <ogc/mutex.h>

extern bool reset_pressed;
extern bool power_pressed;
extern mutex_t watchdogmutex;
extern int watchdogcounter;

void DVDGekkoTick(bool silent);
bool DVDGekkoMount();

void plat_init (int *argc, char **argv[]);
void plat_deinit (int rc);

#define WATCH_TIMEOUT 5
static inline void setwatchdogcounter(int counter) // -1 disable watchdog
{
	if(watchdogmutex==LWP_MUTEX_NULL) return;
	LWP_MutexLock(watchdogmutex);
	//printf("watchdogcounter: %i\n",counter);
	watchdogcounter=counter;
	LWP_MutexUnlock(watchdogmutex);
}


#endif

#endif
