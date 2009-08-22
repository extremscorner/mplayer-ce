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

struct SCESettings CESettings;
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

static lwp_t mainthread;
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

extern bool controlledbygui;

static bool play_end;

static void *
mplayerthread (void *arg)
{	
	mplayer_loadfile(loadedFile);
	loadedFile[0] = 0;
	play_end=true;
	LWP_JoinThread(mainthread,NULL);
	return NULL;
}
extern GXRModeObj *vmode;
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
	
//	DI_Init();	// first (not need is called inside WIIDVD_Init)

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

	log_console_init(vmode, 0); //to debug with usbgecko (all printf are send to usbgecko)


	// store path app was loaded from
	sprintf(appPath, "sd:/apps/mplayer_ce");
	//if(argc > 0 && argv[0] != NULL)
	//	CreateAppPath(argv[0]);

	MountAllFAT(); // Initialize libFAT for SD and USB


	mainthread=LWP_GetSelf();

	LoadConfig(appPath);
	loadedFile[0] = 0;

	// Initialize font system
	InitFreeType((u8*)font_ttf, font_ttf_size);
	
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

		// load video
		VIDEO_SetPostRetraceCallback (NULL); //disable callback in mplayer, reasigned in ResetVideo_Menu

		
		controlledbygui = false;
		if(mthread == LWP_THREAD_NULL)
		{
			play_end=false;
			LWP_CreateThread (&mthread, mplayerthread, NULL, mstack, TSTACK, 69);
		}
		
		while(!controlledbygui && !play_end) // wait for MPlayer to pause
			usleep(9000);
		if(play_end) 
		{
			usleep(1000); //to be sure mplayerthread is joinned
			mthread = LWP_THREAD_NULL;
		}
	
		//log_console_enable_video(true);
		//printf("test\n");sleep(5);
	}
}
