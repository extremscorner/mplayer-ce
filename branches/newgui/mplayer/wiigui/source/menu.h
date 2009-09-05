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

void WiiMenu ();
void ErrorPrompt(const char * msg);
int ErrorPromptRetry(const char * msg);
void InfoPrompt(const char * msg);
void ShowAction (const char *msg);
void CancelAction();
void ShowProgress (const char *msg, int done, int total);

enum
{
	MENU_BROWSE,
	MENU_DVD,
	MENU_ONLINEMEDIA,
	MENU_SETTINGS,
	MENU_SETTINGS_GENERAL,
	MENU_SETTINGS_CACHE,
	MENU_SETTINGS_NETWORK,
	MENU_SETTINGS_VIDEO,
	MENU_SETTINGS_AUDIO,
	MENU_SETTINGS_SUBTITLES
};

#endif
