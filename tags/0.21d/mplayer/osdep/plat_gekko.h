#ifndef __PLAT_GEKKO_H__
#define __PLAT_GEKKO_H__

#ifdef GEKKO

#include <sys/time.h>
#include <sys/dir.h>
#include <unistd.h>

#include <fat.h>

#include <gctypes.h>

//typedef DIR_ITER DIR;
//#include "gekko_dirent.h"

extern bool reset_pressed;
extern bool power_pressed;

// yeah, i know...
// there is no _FILE_OFFSET_BITS in newlib and mplayer expects a 64bit off_t,
// so thats what it gets
#define off_t s64

#define PATH_MAX MAXPATHLEN

int gekko_gettimeofday(struct timeval *tv, void *tz);

void gekko_abort(void);

#define gettimeofday(TV, TZ) gekko_gettimeofday((TV), (TZ))
#define abort(x) gekko_abort(x)

void plat_init (int *argc, char **argv[]);
void plat_deinit (int rc);

#endif

#endif
