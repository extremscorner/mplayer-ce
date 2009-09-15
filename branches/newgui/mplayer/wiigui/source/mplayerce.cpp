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
#include "settings.h"

int ScreenshotRequested = 0;
int ConfigRequested = 0;
int ShutdownRequested = 0;
int ResetRequested = 0;
int ExitRequested = 0;
char appPath[1024];
char loadedFile[1024];

#define TSTACK (512*1024)
static lwp_t mthread = LWP_THREAD_NULL;
static u8 mstack[TSTACK] ATTRIBUTE_ALIGN (32);

/****************************************************************************
 * Shutdown / Reboot / Exit
 ***************************************************************************/

void ExitApp()
{
	SaveSettings(SILENT);

	// shut down some threads
	HaltDeviceThread();
	CancelAction();
	ShutoffRumble();
	StopGX();

	if(ShutdownRequested == 1)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0); // Shutdown Wii

	if(CESettings.exitAction == EXIT_AUTO)
	{
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
	else if(CESettings.exitAction == EXIT_WIIMENU)
	{
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	}
	else
	{
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
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

static void *
mplayerthread (void *arg)
{
	while(1)
	{
		LWP_SuspendThread(mthread);	
		printf("load file: %s\n",loadedFile);			
		if(loadedFile[0] != 0)
			mplayer_loadfile(loadedFile);
		controlledbygui=1;
	}
	return NULL;
}

void LoadMPlayer()
{
	HaltDeviceThread();
	printf("return control to mplayer\n");
	controlledbygui = 0;
	if(LWP_ThreadIsSuspended(mthread))
		LWP_ResumeThread(mthread);
}

void ShutdownMPlayer()
{
	controlledbygui=2;
	while(!LWP_ThreadIsSuspended(mthread))
		usleep(500);
}

int
main(int argc, char *argv[])
{
	int mload=-1;

	//try to load ios202
	if(IOS_GetVersion()!=202)
	{
		if(FindIOS(202))
		{
			IOS_ReloadIOS(202);
			WIIDVD_Init(false);  //dvdx not needed
		}
		else WIIDVD_Init(true);
	}
	else WIIDVD_Init(false);

	//load usb2 driver
	mload=mload_init();
	if(mload<0) DisableUSB2(true);
	else if(!load_ehci_module()) DisableUSB2(true);

	VIDEO_Init();
	PAD_Init();
	WPAD_Init();
	InitVideo(); // Initialise video
	AUDIO_Init(NULL);

	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);

	// Wii Power/Reset buttons
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownCB);
	SYS_SetPowerCallback(ShutdownCB);
	SYS_SetResetCallback(ResetCB);

	extern GXRModeObj *vmode;
	log_console_init(vmode, 0); //to debug with usbgecko (all printf are send to usbgecko, is in libmplayerwii.a)

	// store path app was loaded from
	sprintf(appPath, "sd:/apps/mplayer_ce");
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);

	// Set defaults
	DefaultSettings();

	MountAllFAT(); // Initialize libFAT for SD and USB

	loadedFile[0] = 0;

	// Initialize font system
	InitFreeType((u8*)font_ttf, font_ttf_size);

	// create mplayer thread
	LWP_CreateThread (&mthread, mplayerthread, NULL, mstack, TSTACK, 69);

	while(1)
	{
		AUDIO_RegisterDMACallback(NULL);
		AUDIO_StopDMA();
		ResetVideo_Menu();

		ResumeDeviceThread();
		WiiMenu();

		printf("wait for MPlayer to pause or finish film\n");
		while(!controlledbygui) // wait for MPlayer to pause or finish film
			usleep(9000);

		printf("control return to gui\n");

		//log_console_enable_video(true);
		//printf("test\n");sleep(5);
	}
}
