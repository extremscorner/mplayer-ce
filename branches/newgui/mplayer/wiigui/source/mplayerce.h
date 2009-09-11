/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * mplayer.h
 ***************************************************************************/

#ifndef _MPLAYERGUI_H_
#define _MPLAYERGUI_H_

#include "FreeTypeGX.h"
#include "network.h"
#include "../../osdep/libdvdiso.h"
#include "../../osdep/mload.h"

#define APPNAME			"MPlayer CE"
#define APPVERSION		"1.0.0"
#define APPFOLDER		"apps/mplayer_ce"

enum {
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_DVD,
	DEVICE_SMB,
	DEVICE_FTP
};

#define NOTSILENT 0
#define SILENT 1

void ExitApp();
void LoadMPlayer();
void ShutdownMPlayer();
extern int ScreenshotRequested;
extern int ConfigRequested;
extern int ShutdownRequested;
extern int ExitRequested;
extern FreeTypeGX *fontSystem[];
extern char loadedFile[];
extern char appPath[];

#ifdef __cplusplus
extern "C" {
#endif

extern int controlledbygui;

int mplayer_loadfile(const char* _file);
bool DVDGekkoMount();
void log_console_init(GXRModeObj *vmode, u16 logsize);
//void log_console_deinit(void);
//void log_console_enable_log(bool enable);
void log_console_enable_video(bool enable);

bool FindIOS(u32 ios);   //in plat_gekko.c
bool load_ehci_module();  //in plat_gekko.c
void DisableUSB2(bool);  //in special libogc, in usb2storage.c (need to be added to .h)

#ifdef __cplusplus
}
#endif

#endif
