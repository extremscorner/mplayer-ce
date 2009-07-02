/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * menu.h
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#ifndef _MENU_H_
#define _MENU_H_

#include <ogcsys.h>

void Menu (int menuitem);
void ErrorPrompt(const char * msg);
int ErrorPromptRetry(const char * msg);
void InfoPrompt(const char * msg);
void ShowAction (const char *msg);
void CancelAction();
void ShowProgress (const char *msg, int done, int total);

enum
{
	MENU_EXIT = -1,
	MENU_NONE,
	MENU_MAIN,
	MENU_BROWSE,
	MENU_BROWSE_DEVICE,
	MENU_DVD,
	MENU_RADIO,
	MENU_OPTIONS,
	MENU_OPTIONS_VIDEO,
	MENU_OPTIONS_AUDIO,
	MENU_OPTIONS_SUBTITLES,
	MENU_OPTIONS_MENU,
	MENU_HOME
};

#endif
