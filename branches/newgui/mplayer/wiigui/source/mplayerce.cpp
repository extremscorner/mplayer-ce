/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * mplayer.cpp
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <di/di.h>

#include "FreeTypeGX.h"
#include "video.h"
#include "menu.h"
#include "input.h"
#include "filelist.h"
#include "fileop.h"
#include "mplayerce.h"

FreeTypeGX *fontSystem;
struct SCESettings CESettings;
int ScreenshotRequested = 0;
int ConfigRequested = 0;
int ShutdownRequested = 0;
int ResetRequested = 0;
int ExitRequested = 0;
char appPath[1024];
char loadedFile[1024];

/****************************************************************************
 * Shutdown / Reboot / Exit
 ***************************************************************************/

static void ExitCleanup()
{
	ShutoffRumble();
	StopGX();
	HaltDeviceThread();
	UnmountAllFAT();
	DI_Close();
}

void ExitApp()
{
	ExitCleanup();

	if(ShutdownRequested == 1)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0); // Shutdown Wii

	char * sig = (char *)0x80001804;
	if(
		sig[0] == 'S' &&
		sig[1] == 'T' &&
		sig[2] == 'U' &&
		sig[3] == 'B' &&
		sig[4] == 'H' &&
		sig[5] == 'A' &&
		sig[6] == 'X' &&
		sig[7] == 'X')
		exit(0); // Exit to HBC
	else
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0); // HBC not found
}

void ShutdownCB()
{
	ConfigRequested = 1;
	ShutdownRequested = 1;
}
void ResetCB()
{
	ResetRequested = 1;
}

static void CreateAppPath(char origpath[])
{
	char path[1024];
	strcpy(path, origpath); // make a copy so we don't mess up original

	char * loc;
	int pos = -1;

	loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	loc = strchr(path,'/'); // looking for / from fat:/ or sd:/
	if (loc != NULL)
		pos = loc - path + 1;

	if(pos >= 0 && pos < 1024)
		sprintf(appPath, &(path[pos]));
}

int
main(int argc, char *argv[])
{
	DI_Init();	// first

	VIDEO_Init();
	PAD_Init();
	WPAD_Init();
	InitVideo(); // Initialise video
	AUDIO_Init(NULL);

	//extern GXRModeObj *vmode;
	//log_console_init(vmode, 0);
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);

	// Wii Power/Reset buttons
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownCB);
	SYS_SetPowerCallback(ShutdownCB);
	SYS_SetResetCallback(ResetCB);

	// store path app was loaded from
	sprintf(appPath, "sd:/apps/mplayer_ce");
	//if(argc > 0 && argv[0] != NULL)
	//	CreateAppPath(argv[0]);

	MountAllFAT(); // Initialize libFAT for SD and USB

	LoadConfig(appPath);
	loadedFile[0] = 0;

	// Initialize font system
	fontSystem = new FreeTypeGX();
	fontSystem->loadFont(font_ttf, font_ttf_size, 0);
	fontSystem->setCompatibilityMode(FTGX_COMPATIBILITY_DEFAULT_TEVOP_GX_PASSCLR | FTGX_COMPATIBILITY_DEFAULT_VTXDESC_GX_NONE);

	while(1)
	{
		AUDIO_RegisterDMACallback(NULL);
		AUDIO_StopDMA();
		ResetVideo_Menu();

		ResumeDeviceThread();
		if(strlen(loadedFile) > 0)
			Menu(MENU_HOME);
		else
			Menu(MENU_MAIN);
		HaltDeviceThread();

		//log_console_enable_video(true);
		// load video
		VIDEO_SetPostRetraceCallback (NULL);//disable callback in mplayer, reasigned in ResetVideo_Menu
		mplayer_loadfile(loadedFile);
		//log_console_enable_video(true);
		//printf("test\n");sleep(5);
	}
}
