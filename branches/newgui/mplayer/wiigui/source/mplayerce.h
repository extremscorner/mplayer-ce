/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * mplayer.h
 ***************************************************************************/

#ifndef _MPLAYERGUI_H_
#define _MPLAYERGUI_H_

#include "FreeTypeGX.h"

enum {
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_DVD,
	DEVICE_SMB
};

struct SCESettings {
    int		frameDropping;
    int		aspectRatio;
};

extern struct SCESettings CESettings;

#define NOTSILENT 0
#define SILENT 1

void ExitApp();
extern int ExitRequested;
extern FreeTypeGX *fontSystem;
extern char loadedFile[];

#ifdef __cplusplus
extern "C" {
#endif

int main2 (int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif
