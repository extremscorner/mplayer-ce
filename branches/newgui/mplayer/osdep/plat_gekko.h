#ifndef __PLAT_GEKKO_H__
#define __PLAT_GEKKO_H__

#ifdef GEKKO

#include <sys/time.h>
#include <sys/dir.h>
#include <unistd.h>

#include <fat.h>

#include <gctypes.h>

extern bool reset_pressed;
extern bool power_pressed;


#define PATH_MAX MAXPATHLEN


void gekko_abort(void);
bool DVDGekkoMount();

#define abort(x) gekko_abort(x)

void plat_init (int *argc, char **argv[]);
void plat_deinit (int rc);

#endif

#endif
