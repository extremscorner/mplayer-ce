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

#include "FreeTypeGX.h"
#include "video.h"
#include "menu.h"
#include "input.h"
#include "filelist.h"
#include "fileop.h"
#include "mplayerce.h"

FreeTypeGX *fontSystem;
struct SCESettings CESettings;
int ExitRequested = 0;

void ExitApp()
{
	ShutoffRumble();
	StopGX();
	exit(0);
}

int
main(int argc, char *argv[])
{
	PAD_Init();
	WPAD_Init();
	InitVideo(); // Initialise video

	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);

	// Initialize font system
	fontSystem = new FreeTypeGX();
	fontSystem->loadFont(font_ttf, font_ttf_size, 0);
	fontSystem->setCompatibilityMode(FTGX_COMPATIBILITY_DEFAULT_TEVOP_GX_PASSCLR | FTGX_COMPATIBILITY_DEFAULT_VTXDESC_GX_NONE);

	InitGUIThreads();
	InitDeviceThread();
	Menu(MENU_MAIN);
}
