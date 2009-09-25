/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * menu.cpp
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "libwiigui/gui.h"
#include "menu.h"
#include "mplayerce.h"
#include "settings.h"
#include "fileop.h"
#include "input.h"
#include "networkop.h"
#include "filebrowser.h"
#include "filelist.h"

#define THREAD_SLEEP 100

static GuiImageData * pointer[4] = { NULL, NULL, NULL, NULL };
static GuiImage * videoImg = NULL;
static GuiButton * videoBtn = NULL;
static GuiButton * musicBtn = NULL;
static GuiButton * dvdBtn = NULL;
static GuiButton * onlineBtn = NULL;
static GuiButton * configBtn = NULL;
static GuiButton * logoBtn = NULL;
static GuiButton * mplayerBtn = NULL;
static GuiWindow * mainWindow = NULL;
static GuiText * settingText = NULL;

int currentMenu = MENU_BROWSE_VIDEOS;
static int lastMenu = MENU_BROWSE_VIDEOS;
static int netEditIndex = 0; // current index of FTP/SMB share being edited

static lwp_t guithread = LWP_THREAD_NULL;
static lwp_t progressthread = LWP_THREAD_NULL;
static lwp_t creditsthread = LWP_THREAD_NULL;
static lwp_t updatethread = LWP_THREAD_NULL;
static bool guiHalt = true;
static bool guiShutdown = true;
static int showProgress = 0;

static char progressTitle[100];
static char progressMsg[200];
static int progressDone = 0;
static int progressTotal = 0;

static bool creditsOpen = false;

int doMPlayerGuiDraw = 0; // draw MPlayer menu
static bool menuMode = 0; // 0 - normal GUI, 1 - GUI for MPlayer

/****************************************************************************
 * AppUpdate
 *
 * Prompts for confirmation, and downloads/installs updates
 ***************************************************************************/
static void *
AppUpdate (void *arg)
{
	bool installUpdate = WindowPrompt(
		"Update Available",
		"An update is available!",
		"Update now",
		"Update later");
	if(installUpdate)
		if(DownloadUpdate())
			ExitRequested = 1;
	return NULL;
}

/****************************************************************************
 * UpdateGui
 *
 * Primary GUI thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/
static void *
UpdateGui (void *arg)
{
	while(1)
	{
		if(guiHalt)
		{
			break;
		}
		else
		{
			while(menuMode == 1 && !doMPlayerGuiDraw) // mplayer GUI
			{
				usleep(THREAD_SLEEP);
				if(guiHalt)
					return NULL;
			}

			UpdatePads();
			mainWindow->Draw();

			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad.ir.valid)
					Menu_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48,
						96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, 1, 1, 255);
				DoRumble(i);
			}

			for(int i=0; i < 4; i++)
				mainWindow->Update(&userInput[i]);
			
			if(menuMode == 0) // normal GUI
			{
				Menu_Render();

				if(updateFound)
				{
					updateFound = false;
					LWP_CreateThread (&updatethread, AppUpdate, NULL, NULL, 0, 70);
				}
				
				if(!creditsOpen && creditsthread != LWP_THREAD_NULL)
				{
					LWP_JoinThread(creditsthread, NULL);
					creditsthread = LWP_THREAD_NULL;
				}
	
				if(userInput[0].wpad.btns_d & WPAD_BUTTON_HOME)
					ExitRequested = 1;
	
				if(ExitRequested || ShutdownRequested)
				{
					for(int a = 0; a < 255; a += 15)
					{
						mainWindow->Draw();
						Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, a},1);
						Menu_Render();
					}
					ExitApp();
				}
			}
			else // MPlayer GUI
			{
				doMPlayerGuiDraw = 0;
				usleep(THREAD_SLEEP);
			}
		}
	}
	return NULL;
}

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void
ResumeGui()
{
	guiHalt = false;

	if(guithread == LWP_THREAD_NULL)
		LWP_CreateThread (&guithread, UpdateGui, NULL, NULL, 0, 70);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

static void
HaltGui()
{
	guiHalt = true;

	if(guithread == LWP_THREAD_NULL)
		return;

	// wait for thread to finish
	LWP_JoinThread(guithread, NULL);
	guithread = LWP_THREAD_NULL;
}

void ShutdownGui()
{
	CancelAction();
	HaltGui();
	guiShutdown = true;
}

#ifdef __cplusplus
}
#endif
/****************************************************************************
 * WindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice
 ***************************************************************************/
int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){0, 0, 0, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 430);

	GuiText btn1Txt(btn1Label, 22, (GXColor){255, 255, 255, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -40);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -40);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (GXColor){255, 255, 255, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -40);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetTrigger(&trigA);
	btn2.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	CancelAction();
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		usleep(THREAD_SLEEP);

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(THREAD_SLEEP);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}

/****************************************************************************
 * ProgressWindow
 *
 * Opens a window, which displays progress to the user. Can either display a
 * progress bar showing % completion, or a throbber that only shows that an
 * action is in progress.
 ***************************************************************************/
static void
ProgressWindow(char *title, char *msg)
{
	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiImageData progressbarOutline(progressbar_outline_png);
	GuiImage progressbarOutlineImg(&progressbarOutline);
	progressbarOutlineImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarOutlineImg.SetPosition(25, 40);

	GuiImageData progressbarEmpty(progressbar_empty_png);
	GuiImage progressbarEmptyImg(&progressbarEmpty);
	progressbarEmptyImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarEmptyImg.SetPosition(25, 40);
	progressbarEmptyImg.SetTile(100);

	GuiImageData progressbar(progressbar_png);
	GuiImage progressbarImg(&progressbar);
	progressbarImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarImg.SetPosition(25, 40);

	GuiImageData throbber(throbber_png);
	GuiImage throbberImg(&throbber);
	throbberImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	throbberImg.SetPosition(0, 40);

	GuiText titleTxt(title, 26, (GXColor){0, 0, 0, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);
	GuiText msgTxt(msg, 26, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,80);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);

	if(showProgress == 1)
	{
		promptWindow.Append(&progressbarEmptyImg);
		promptWindow.Append(&progressbarImg);
		promptWindow.Append(&progressbarOutlineImg);
	}
	else
	{
		promptWindow.Append(&throbberImg);
	}

	usleep(400000); // wait to see if progress flag changes soon
	if(!showProgress)
		return;

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	float angle = 0;
	u32 count = 0;

	while(showProgress)
	{
		usleep(20000);

		if(showProgress == 1)
		{
			progressbarImg.SetTile(100*progressDone/progressTotal);
		}
		else if(showProgress == 2)
		{
			if(count % 5 == 0)
			{
				angle+=45;
				if(angle >= 360)
					angle = 0;
				throbberImg.SetAngle(angle);
			}
			count++;
		}
	}

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}

static void * ProgressThread (void *arg)
{
	while(1)
	{
		if(!showProgress)
			break;

		ProgressWindow(progressTitle, progressMsg);
		usleep(THREAD_SLEEP);
	}
	return NULL;
}

/****************************************************************************
 * CancelAction
 *
 * Signals the GUI progress window thread to halt, and waits for it to
 * finish. Prevents multiple progress window events from interfering /
 * overriding each other.
 ***************************************************************************/
void
CancelAction()
{
	showProgress = 0;

	if(progressthread == LWP_THREAD_NULL)
		return;

	// wait for thread to finish
	LWP_JoinThread(progressthread, NULL);
	progressthread = LWP_THREAD_NULL;
}

/****************************************************************************
 * ShowProgress
 *
 * Updates the variables used by the progress window for drawing a progress
 * bar. Also resumes the progress window thread if it is suspended.
 ***************************************************************************/
void
ShowProgress (const char *msg, int done, int total)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
		return;

	if(total < (256*1024))
		return;
	else if(done > total) // this shouldn't happen
		done = total;

	if(done/total > 0.99)
		done = total;

	if(showProgress != 1)
		CancelAction(); // wait for previous progress window to finish

	strncpy(progressMsg, msg, 200);
	sprintf(progressTitle, "Please Wait");
	showProgress = 1;
	progressTotal = total;
	progressDone = done;

	if(progressthread == LWP_THREAD_NULL)
		LWP_CreateThread (&progressthread, ProgressThread, NULL, NULL, 0, 40);
}

/****************************************************************************
 * ShowAction
 *
 * Shows that an action is underway. Also resumes the progress window thread
 * if it is suspended.
 ***************************************************************************/
void
ShowAction (const char *msg)
{
	if(!mainWindow || ExitRequested || ShutdownRequested)
		return;

	if(showProgress != 2)
		CancelAction(); // wait for previous progress window to finish

	strncpy(progressMsg, msg, 200);
	sprintf(progressTitle, "Please Wait");
	showProgress = 2;
	progressDone = 0;
	progressTotal = 0;

	if(progressthread == LWP_THREAD_NULL)
		LWP_CreateThread (&progressthread, ProgressThread, NULL, NULL, 0, 40);
}

void ErrorPrompt(const char *msg)
{
	WindowPrompt("Error", msg, "OK", NULL);
}

int ErrorPromptRetry(const char *msg)
{
	return WindowPrompt("Error", msg, "Retry", "Cancel");
}

void InfoPrompt(const char *msg)
{
	WindowPrompt("Information", msg, "OK", NULL);
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, u16 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 22, (GXColor){255, 255, 255, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){255, 255, 255, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}

/****************************************************************************
 * SettingWindow
 *
 * Opens a new window, with the specified window element appended. Allows
 * for a customizable prompted setting.
 ***************************************************************************/
static int
SettingWindow(const char * title, GuiWindow * w)
{
	int save = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){70, 70, 10, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);

	GuiText okBtnTxt("OK", 24, (GXColor){255, 255, 255, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(20, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 24, (GXColor){255, 255, 255, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-20, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&okBtn);
	promptWindow.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->Append(w);
	mainWindow->ChangeFocus(w);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->Remove(w);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return save;
}

/****************************************************************************
 * WindowCredits
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/
static void * WindowCredits(void *arg)
{
	bool exit = false;
	int i = 0;
	int y = 20;

	GuiWindow creditsWindow(580,448);
	creditsWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	creditsWindow.Append(&creditsBoxImg);

	int numEntries = 23;
	GuiText * txt[numEntries];

	txt[i] = new GuiText("Credits", 30, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=32;

	txt[i] = new GuiText("Official Site: http://code.google.com/p/mplayer-ce/", 20, (GXColor){0, 0, 0, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=40;

	txt[i]->SetPresets(20, (GXColor){0, 0, 0, 255}, 0,
			FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP, ALIGN_LEFT, ALIGN_TOP);

	txt[i] = new GuiText("rodries");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Coding");
	txt[i]->SetPosition(320,y); i++; y+=22;
	txt[i] = new GuiText("Tantric");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Coding & menu design");
	txt[i]->SetPosition(320,y); i++; y+=22;
	txt[i] = new GuiText("drmr");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Menu artwork");
	txt[i]->SetPosition(320,y); i++; y+=22;
	txt[i] = new GuiText("scip");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Original project author");
	txt[i]->SetPosition(320,y); i++; y+=22;
	txt[i] = new GuiText("AgentX");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Coding & testing");
	txt[i]->SetPosition(320,y); i++; y+=22;
	txt[i] = new GuiText("DJDynamite123");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("Testing");
	txt[i]->SetPosition(320,y); i++; y+=44;

	txt[i] = new GuiText("Thanks also to:");
	txt[i]->SetPosition(50,y); i++; y+=36;

	txt[i] = new GuiText("MPlayer Team");
	txt[i]->SetPosition(50,y); i++; y+=22;

	txt[i] = new GuiText("libogc / devkitPPC");
	txt[i]->SetPosition(50,y); i++;
	txt[i] = new GuiText("shagkur & wintermute");
	txt[i]->SetPosition(320,y); i++; y+=36;

	txt[i] = new GuiText("Team Twiizers, Armin Tamzarian, Daca, dargllun,");
	txt[i]->SetPosition(50,y); i++; y+=22;
	txt[i] = new GuiText("Extrems, GeeXboX Authors, hax, Shareese, tipolosko");
	txt[i]->SetPosition(50,y); i++; y+=22;

	txt[i]->SetPresets(18, (GXColor){0, 0, 0, 255}, 0,
		FTGX_JUSTIFY_CENTER | FTGX_ALIGN_TOP, ALIGN_CENTRE, ALIGN_TOP);

	txt[i] = new GuiText("This software is open source and may be copied,");
	txt[i]->SetPosition(0,y); i++; y+=20;
	txt[i] = new GuiText("distributed, or modified under the terms of the");
	txt[i]->SetPosition(0,y); i++; y+=20;
	txt[i] = new GuiText("GNU General Public License (GPL) Version 2.");
	txt[i]->SetPosition(0,y); i++; y+=20;

	for(i=0; i < numEntries; i++)
		creditsWindow.Append(txt[i]);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&creditsWindow);
	mainWindow->ChangeFocus(&creditsWindow);
	ResumeGui();
	
	while(!exit)
	{
		for(i=0; i < 4; i++)
		{
			if(userInput[i].wpad.btns_d || userInput[i].pad.btns_d)
				exit = true;
		}
		usleep(THREAD_SLEEP);
	}

	HaltGui();
	mainWindow->Remove(&creditsWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	
	for(i=0; i < numEntries; i++)
		delete txt[i];
	creditsOpen = false;
	return NULL;
}

static void DisplayCredits(void * ptr)
{
	if(logoBtn->GetState() != STATE_CLICKED)
		return;

	logoBtn->ResetState();
	
	// spawn a new thread to handle the Credits
	creditsOpen = true;
	if(creditsthread == LWP_THREAD_NULL)
		LWP_CreateThread (&creditsthread, WindowCredits, NULL, NULL, 0, 70);
}

static void ChangeMenu(int menu)
{
	lastMenu = currentMenu;
	currentMenu = menu;
}
static void ChangeMenu(void * ptr, int menu)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		ChangeMenu(menu);
		b->ResetState();
	}
}
static void ChangeMenuVideos(void * ptr) { ChangeMenu(ptr, MENU_BROWSE_VIDEOS); }
static void ChangeMenuMusic(void * ptr) { ChangeMenu(ptr, MENU_BROWSE_MUSIC); }
static void ChangeMenuDVD(void * ptr) { ChangeMenu(ptr, MENU_DVD); }
static void ChangeMenuOnline(void * ptr) { ChangeMenu(ptr, MENU_BROWSE_ONLINEMEDIA); }
static void ChangeMenuSettings(void * ptr) { ChangeMenu(ptr, MENU_SETTINGS); }

/****************************************************************************
 * MenuBrowse
 ***************************************************************************/

static void MenuBrowse(int menu)
{
	ShutoffRumble();

	if(menu == MENU_BROWSE_VIDEOS)
	{
		browser.dir = &CESettings.videoFolder[0];
		inOnlineMedia = false;
	}
	else if(menu == MENU_BROWSE_MUSIC)
	{
		browser.dir = &CESettings.musicFolder[0];
		inOnlineMedia = false;
	}
	else if(menu == MENU_BROWSE_ONLINEMEDIA)
	{
		browser.dir = &CESettings.onlinemediaFolder[0];
		inOnlineMedia = true;
	}
	else
		return;

	// populate initial directory listing
	while(BrowserChangeFolder(false) <= 0)
	{
		int choice = WindowPrompt(
		"Error",
		"Unable to display files on selected load device.",
		"Retry",
		"Check Settings");

		if(choice == 0)
		{
			ChangeMenu(MENU_SETTINGS);
			return;
		}
	}

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiFileBrowser fileBrowser(480, 300);
	fileBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	fileBrowser.SetPosition(10, 100);

	HaltGui();
	mainWindow->Append(&fileBrowser);
	ResumeGui();

	while(currentMenu == menu && !guiShutdown)
	{
		usleep(THREAD_SLEEP);
		
		if(selectLoadedFile == 2)
		{
			selectLoadedFile = 0;
			fileBrowser.TriggerUpdate();
		}

		// update file browser based on arrow buttons
		// request guiShutdown if A button pressed on a file
		for(int i=0; i<FILE_PAGESIZE; i++)
		{
			if(fileBrowser.fileList[i]->GetState() == STATE_CLICKED)
			{
				fileBrowser.fileList[i]->ResetState();
				// check corresponding browser entry
				if(browserList[browser.selIndex].isdir)
				{
					if(BrowserChangeFolder())
					{
						fileBrowser.ResetState();
						fileBrowser.fileList[0]->SetState(STATE_SELECTED);
						fileBrowser.TriggerUpdate();
					}
					else
					{
						goto done;
					}
				}
				else
				{
					if(browserList[browser.selIndex].isplaylist)
					{
						// parse list
						if(currentMenu == MENU_BROWSE_ONLINEMEDIA)
							sprintf(currentPlaylist, "%s", browserList[browser.selIndex].filename);
						else
							sprintf(currentPlaylist, "%s%s", browser.dir, browserList[browser.selIndex].filename);
						
						ShowAction("Loading playlist...");
						int numItems = LoadPlaylist();
						CancelAction();
						
						if(numItems == 0)
						{
							currentPlaylist[0] = 0;
							ErrorPrompt("Error loading playlist!");
							continue;
						}
						BrowserChangeFolder();
						
						if(numItems == 1) // let's load this one file
						{
							sprintf(loadedFile, browserList[1].filename);
							// go up one level
							browser.selIndex = 0;
							BrowserChangeFolder();
						}
						else
						{
							fileBrowser.ResetState();
							fileBrowser.fileList[0]->SetState(STATE_SELECTED);
							fileBrowser.TriggerUpdate();
							continue;
						}
					}
					else
					{
						if(currentPlaylist[0] != 0 || currentMenu == MENU_BROWSE_ONLINEMEDIA)
							sprintf(loadedFile, "%s", browserList[browser.selIndex].filename);
						else
							sprintf(loadedFile, "%s%s", browser.dir, browserList[browser.selIndex].filename);
					}
					
					ShutdownMPlayer();
					
					ShowAction("Loading...");

					// signal MPlayer to load
					LoadMPlayer();

					// wait until MPlayer is ready to take control (or return control)
					while(!guiShutdown && controlledbygui != 1)
						usleep(THREAD_SLEEP);
					
					CancelAction();

					if(guiShutdown)
					{
						goto done;
					}
					else
					{
						// we loaded an audio file - if we already had a video
						// loaded, we should remove the bg and MPlayer button
						mainWindow->Remove(videoImg);
						mainWindow->Remove(mplayerBtn);
					}
				}
			}
		}
	}
done:
	HaltGui();
	mainWindow->Remove(&fileBrowser);
}

static void MenuDVD()
{
	int ret;
	int i = 0;
	int selected = -1;

	MenuItemList items;
	sprintf(items.name[i], "Play Title #1");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Play Title #2");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Play Title #3");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Play Title #4");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Play Title #5");
	items.img[i] = NULL; i++;
	items.length = i;

	GuiText titleTxt("DVD", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiMenuBrowser itemBrowser(300, 400, &items);
	itemBrowser.SetPosition(30, 120);
	itemBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	mainWindow->Append(&itemBrowser);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	if(!ChangeInterface(DEVICE_DVD, -1, NOTSILENT))
		ChangeMenu(lastMenu); // go back to last menu

	while(currentMenu == MENU_DVD && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		if(selected != itemBrowser.GetSelectedItem())
		{
			selected = itemBrowser.GetSelectedItem();
		}

		ret = itemBrowser.GetClickedItem();

		if(ret >= 0)
		{
			sprintf(loadedFile, "dvd://%d", ret+1);

			ShutdownMPlayer();

			ShowAction("Loading...");

			// signal MPlayer to load
			LoadMPlayer();

			// wait until MPlayer is ready to take control
			while(!guiShutdown)
				usleep(THREAD_SLEEP);

			CancelAction();
			guiShutdown = true;
		}
	}

	HaltGui();
	mainWindow->Remove(&itemBrowser);
	mainWindow->Remove(&titleTxt);
}

static void MenuSettingsGeneral()
{
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Auto-Resume");
	sprintf(options.name[i++], "Play Order");
	sprintf(options.name[i++], "Clean Filenames");
	sprintf(options.name[i++], "File Extensions");
	sprintf(options.name[i++], "Unsupported Files");
	sprintf(options.name[i++], "Language");
	sprintf(options.name[i++], "Video Files Folder");
	sprintf(options.name[i++], "Music Files Folder");
	sprintf(options.name[i++], "Exit Action");
	sprintf(options.name[i++], "Wiimote Rumble");

	options.length = i;
		
	for(i=0; i < options.length; i++)
	{
		options.value[i][0] = 0;
		options.icon[i] = 0;
	}

	GuiText titleTxt("Settings - General", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol2Position(200);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_GENERAL && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				CESettings.autoResume ^= 1;
				break;
			case 1:
				CESettings.playOrder++;
				if(CESettings.playOrder > PLAY_LOOP)
					CESettings.playOrder = 0;
				break;
			case 2:
				CESettings.cleanFilenames ^= 1;
				break;
			case 3:
				CESettings.hideExtensions ^= 1;
				break;
			case 4:
				CESettings.filterFiles ^= 1;
				break;
			case 5:
				CESettings.language++;
				if(CESettings.language > LANG_KOREAN)
					CESettings.language = 0;
				break;
			case 6:
				OnScreenKeyboard(CESettings.videoFolder, MAXPATHLEN);
				break;
			case 7:
				OnScreenKeyboard(CESettings.musicFolder, MAXPATHLEN);
				break;
			case 8:
				CESettings.exitAction++;
				if(CESettings.exitAction > EXIT_LOADER)
					CESettings.exitAction = 0;
				break;
			case 9:
				CESettings.rumble ^= 1;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			sprintf(options.value[0], "%s", CESettings.autoResume ? "On" : "Off");

			switch(CESettings.playOrder)
			{
				case PLAY_SINGLE:		sprintf(options.value[1], "Single"); break;
				case PLAY_CONTINUOUS:	sprintf(options.value[1], "Continuous"); break;
				case PLAY_SHUFFLE:		sprintf(options.value[1], "Shuffle"); break;
				case PLAY_LOOP:			sprintf(options.value[1], "Loop"); break;
			}

			sprintf(options.value[2], "%s", CESettings.cleanFilenames ? "On" : "Off");
			sprintf(options.value[3], "%s", CESettings.hideExtensions ? "Hide" : "Show");
			sprintf(options.value[4], "%s", CESettings.filterFiles ? "Hide" : "Show");

			switch(CESettings.language)
			{
				case LANG_JAPANESE:		sprintf(options.value[5], "Japanese"); break;
				case LANG_ENGLISH:		sprintf(options.value[5], "English"); break;
				case LANG_GERMAN:		sprintf(options.value[5], "German"); break;
				case LANG_FRENCH:		sprintf(options.value[5], "French"); break;
				case LANG_SPANISH:		sprintf(options.value[5], "Spanish"); break;
				case LANG_ITALIAN:		sprintf(options.value[5], "Italian"); break;
				case LANG_DUTCH:		sprintf(options.value[5], "Dutch"); break;
				case LANG_SIMP_CHINESE:	sprintf(options.value[5], "Chinese (Simplified)"); break;
				case LANG_TRAD_CHINESE:	sprintf(options.value[5], "Chinese (Traditional)"); break;
				case LANG_KOREAN:		sprintf(options.value[5], "Korean"); break;
			}

			snprintf(options.value[6], 20, "%s", CESettings.videoFolder);
			snprintf(options.value[7], 20, "%s", CESettings.musicFolder);

			switch(CESettings.exitAction)
			{
				case EXIT_AUTO:		sprintf(options.value[8], "Auto"); break;
				case EXIT_WIIMENU:	sprintf(options.value[8], "Return to Wii Menu"); break;
				case EXIT_POWEROFF:	sprintf(options.value[8], "Power Off Wii"); break;
				case EXIT_LOADER:	sprintf(options.value[8], "Return to Loader"); break;
			}

			sprintf(options.value[9], "%s", CESettings.rumble ? "On" : "Off");

			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void MenuSettingsCache()
{
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Size");
	sprintf(options.name[i++], "Prefill");
	sprintf(options.name[i++], "Refill");

	options.length = i;

	for(i=0; i < options.length; i++)
	{
		options.value[i][0] = 0;
		options.icon[i] = 0;
	}

	GuiText titleTxt("Settings - Cache", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol2Position(220);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_CACHE && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				CESettings.cacheSize += 2048;
				if(CESettings.cacheSize > 16384)
					CESettings.cacheSize = 0;
				break;

			case 1:
				CESettings.cacheFillStart += 10;
				if (CESettings.cacheFillStart > 100)
					CESettings.cacheFillStart = 0;
				break;

			case 2:
				CESettings.cacheFillRestart += 10;
				if (CESettings.cacheFillRestart > 100)
					CESettings.cacheFillRestart = 0;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			sprintf (options.value[0], "%d", CESettings.cacheSize);
			sprintf (options.value[1], "%d%%", CESettings.cacheFillStart);
			sprintf (options.value[2], "%d%%", CESettings.cacheFillRestart);

			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void MenuSettingsNetwork()
{
	int ret;
	int i = 0;
	OptionList options;
	
	// find all currently set SMB/FTP entries
	
	for(int j=0; j < 5; j++)
	{
		options.name[i][0] = 0;
		options.icon[i] = ICON_SMB;
		options.value[i][0] = 0;

		if(CESettings.smbConf[j].share[0] != 0)
		{
			if(CESettings.smbConf[j].displayname[0] != 0)
				sprintf(options.name[i], "%s", CESettings.smbConf[j].displayname);
			else
				sprintf(options.name[i], "%s", CESettings.smbConf[j].share);
		}
		i++;
	}
	for(int j=0; j < 5; j++)
	{
		options.name[i][0] = 0;
		options.icon[i] = ICON_FTP;
		options.value[i][0] = 0;

		if(CESettings.ftpConf[j].ip[0] != 0)
		{
			if(CESettings.ftpConf[j].displayname[0] != 0)
				sprintf(options.name[i], "%s", CESettings.ftpConf[j].displayname);
			else
				sprintf(options.name[i], "%s@%s/%s", CESettings.ftpConf[j].user, CESettings.ftpConf[j].ip, CESettings.ftpConf[j].folder);
		}
		i++;
	}

	options.length = i;

	GuiText titleTxt("Settings - Network", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData iconSMB(icon_smb_png);
	GuiImageData iconFTP(icon_ftp_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();
	
	GuiText addsmbBtnTxt("Add", 24, (GXColor){255, 255, 255, 255});
	GuiImage addsmbBtnImg(&iconSMB);
	addsmbBtnImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	addsmbBtnTxt.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	GuiButton addsmbBtn(75, btnOutline.GetHeight());
	addsmbBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	addsmbBtn.SetPosition(250, -35);
	addsmbBtn.SetLabel(&addsmbBtnTxt);
	addsmbBtn.SetImage(&addsmbBtnImg);
	addsmbBtn.SetTrigger(&trigA);
	addsmbBtn.SetEffectGrow();
	
	GuiText addftpBtnTxt("Add", 24, (GXColor){255, 255, 255, 255});
	GuiImage addftpBtnImg(&iconFTP);
	addftpBtnImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	addftpBtnTxt.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	GuiButton addftpBtn(75, btnOutline.GetHeight());
	addftpBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	addftpBtn.SetPosition(335, -35);
	addftpBtn.SetLabel(&addftpBtnTxt);
	addftpBtn.SetImage(&addftpBtnImg);
	addftpBtn.SetTrigger(&trigA);
	addftpBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol1Position(30);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	w.Append(&addsmbBtn);
	w.Append(&addftpBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_NETWORK && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		if((ret >= 0 && ret < 5) || addsmbBtn.GetState() == STATE_CLICKED)
		{
			netEditIndex = ret;
			currentMenu = MENU_SETTINGS_NETWORK_SMB;
		}
		else if(ret >= 5 || addftpBtn.GetState() == STATE_CLICKED)
		{
			netEditIndex = ret-5;
			currentMenu = MENU_SETTINGS_NETWORK_FTP;
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void MenuSettingsNetworkSMB()
{
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	char titleStr[100];
	char shareName[100];

	sprintf(options.name[i++], "Display Name");
	sprintf(options.name[i++], "Share IP");
	sprintf(options.name[i++], "Share Name");
	sprintf(options.name[i++], "Username");
	sprintf(options.name[i++], "Password");

	options.length = i;
	
	for(i=0; i < options.length; i++)
	{
		options.value[i][0] = 0;
		options.icon[i] = 0;
	}
	
	if(netEditIndex < 0)
		sprintf(shareName, "New Share");
	else if(CESettings.smbConf[netEditIndex].displayname[0] != 0)
		sprintf(shareName, "%s", CESettings.smbConf[netEditIndex].displayname);
	else
		sprintf(shareName, "%s", CESettings.smbConf[netEditIndex].share);

	sprintf(titleStr, "Settings - Network - %s", shareName);

	GuiText titleTxt(titleStr, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();
	
	GuiText deleteBtnTxt("Delete", 24, (GXColor){255, 255, 255, 255});
	GuiImage deleteBtnImg(&btnOutline);
	GuiImage deleteBtnImgOver(&btnOutlineOver);
	GuiButton deleteBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	deleteBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	deleteBtn.SetPosition(245, -35);
	deleteBtn.SetLabel(&deleteBtnTxt);
	deleteBtn.SetImage(&deleteBtnImg);
	deleteBtn.SetImageOver(&deleteBtnImgOver);
	deleteBtn.SetTrigger(&trigA);
	deleteBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol2Position(220);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	
	if(netEditIndex < 0)
	{
		// find a share to put the data into
		for(i=0; i < 5; i++)
		{
			if(CESettings.smbConf[i].share[0] == 0)
			{
				netEditIndex = i;
				break;
			}
		}
	}
	else
	{
		w.Append(&deleteBtn);
	}
	
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_NETWORK_SMB && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				OnScreenKeyboard(CESettings.smbConf[netEditIndex].displayname, 80);
				break;
			
			case 1:
				OnScreenKeyboard(CESettings.smbConf[netEditIndex].ip, 80);
				break;

			case 2:
				OnScreenKeyboard(CESettings.smbConf[netEditIndex].share, 80);
				break;

			case 3:
				OnScreenKeyboard(CESettings.smbConf[netEditIndex].user, 20);
				break;

			case 4:
				OnScreenKeyboard(CESettings.smbConf[netEditIndex].pwd, 14);
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			strncpy (options.value[0], CESettings.smbConf[netEditIndex].displayname, 80);
			strncpy (options.value[1], CESettings.smbConf[netEditIndex].ip, 80);
			strncpy (options.value[2], CESettings.smbConf[netEditIndex].share, 80);
			strncpy (options.value[3], CESettings.smbConf[netEditIndex].user, 20);
			strncpy (options.value[4], CESettings.smbConf[netEditIndex].pwd, 14);
			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS_NETWORK;
		}
		if(deleteBtn.GetState() == STATE_CLICKED)
		{
			deleteBtn.ResetState();
			if (WindowPrompt("Delete Share", "Are you sure that you want to delete this share?", "OK", "Cancel"))
			{
				CESettings.smbConf[netEditIndex].displayname[0] = 0;
				CESettings.smbConf[netEditIndex].ip[0] = 0;
				CESettings.smbConf[netEditIndex].share[0] = 0;
				CESettings.smbConf[netEditIndex].user[0] = 0;
				CESettings.smbConf[netEditIndex].pwd[0] = 0;
				currentMenu = MENU_SETTINGS_NETWORK;
			}
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void MenuSettingsNetworkFTP()
{
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	char titleStr[100];
	char siteName[100];

	sprintf(options.name[i++], "Display Name");
	sprintf(options.name[i++], "IP");
	sprintf(options.name[i++], "Folder");
	sprintf(options.name[i++], "Username");
	sprintf(options.name[i++], "Password");
	sprintf(options.name[i++], "Mode");

	options.length = i;
		
	for(i=0; i < options.length; i++)
	{
		options.value[i][0] = 0;
		options.icon[i] = 0;
	}
	
	if(netEditIndex < 0)
		sprintf(siteName, "New Site");
	else if(CESettings.ftpConf[netEditIndex].displayname[0] != 0)
		sprintf(siteName, "%s", CESettings.ftpConf[netEditIndex].displayname);
	else
		sprintf(options.name[i], "%s@%s/%s", 
		CESettings.ftpConf[netEditIndex].user, 
		CESettings.ftpConf[netEditIndex].ip, 
		CESettings.ftpConf[netEditIndex].folder);

	sprintf(titleStr, "Settings - Network - %s", siteName);

	GuiText titleTxt(titleStr, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();
	
	GuiText deleteBtnTxt("Delete", 24, (GXColor){255, 255, 255, 255});
	GuiImage deleteBtnImg(&btnOutline);
	GuiImage deleteBtnImgOver(&btnOutlineOver);
	GuiButton deleteBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	deleteBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	deleteBtn.SetPosition(245, -35);
	deleteBtn.SetLabel(&deleteBtnTxt);
	deleteBtn.SetImage(&deleteBtnImg);
	deleteBtn.SetImageOver(&deleteBtnImgOver);
	deleteBtn.SetTrigger(&trigA);
	deleteBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol2Position(220);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	
	if(netEditIndex < 0)
	{
		// find a site to put the data into
		for(i=0; i < 5; i++)
		{
			if(CESettings.ftpConf[i].ip[0] == 0)
			{
				netEditIndex = i;
				break;
			}
		}
	}
	else
	{
		w.Append(&deleteBtn);
	}
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_NETWORK_FTP && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				OnScreenKeyboard(CESettings.ftpConf[netEditIndex].displayname, 80);
				break;
			
			case 1:
				OnScreenKeyboard(CESettings.ftpConf[netEditIndex].ip, 80);
				break;

			case 2:
				OnScreenKeyboard(CESettings.ftpConf[netEditIndex].folder, 80);
				break;

			case 3:
				OnScreenKeyboard(CESettings.ftpConf[netEditIndex].user, 20);
				break;

			case 4:
				OnScreenKeyboard(CESettings.ftpConf[netEditIndex].pwd, 14);
				break;
				
			case 5:
				CESettings.ftpConf[netEditIndex].passive ^= 1;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			strncpy (options.value[0], CESettings.ftpConf[netEditIndex].displayname, 80);
			strncpy (options.value[1], CESettings.ftpConf[netEditIndex].ip, 80);
			strncpy (options.value[2], CESettings.ftpConf[netEditIndex].folder, 80);
			strncpy (options.value[3], CESettings.ftpConf[netEditIndex].user, 20);
			strncpy (options.value[4], CESettings.ftpConf[netEditIndex].pwd, 14);
			sprintf(options.value[5], "%s", CESettings.ftpConf[netEditIndex].passive ? "Passive" : "Active");
			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS_NETWORK;
		}
		if(deleteBtn.GetState() == STATE_CLICKED)
		{
			deleteBtn.ResetState();
			if (WindowPrompt("Delete Site", "Are you sure that you want to delete this site?", "OK", "Cancel"))
			{
				CESettings.ftpConf[netEditIndex].displayname[0] = 0;
				CESettings.ftpConf[netEditIndex].ip[0] = 0;
				CESettings.ftpConf[netEditIndex].folder[0] = 0;
				CESettings.ftpConf[netEditIndex].user[0] = 0;
				CESettings.ftpConf[netEditIndex].pwd[0] = 0;
				CESettings.ftpConf[netEditIndex].passive = 0;
				currentMenu = MENU_SETTINGS_NETWORK;
			}
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void ScreenZoomWindowUpdate(void * ptr, float amount)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		CESettings.videoZoom += amount;

		char zoom[10];
		sprintf(zoom, "%.2f%%", CESettings.videoZoom*100);
		settingText->SetText(zoom);
		b->ResetState();
	}
}

static void ScreenZoomWindowLeftClick(void * ptr) { ScreenZoomWindowUpdate(ptr, -0.01); }
static void ScreenZoomWindowRightClick(void * ptr) { ScreenZoomWindowUpdate(ptr, +0.01); }

static void ScreenZoomWindow()
{
	GuiWindow * w = new GuiWindow(250,250);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenZoomWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenZoomWindowRightClick);

	settingText = new GuiText(NULL, 22, (GXColor){0, 0, 0, 255});
	char zoom[10];
	sprintf(zoom, "%.2f%%", CESettings.videoZoom*100);
	settingText->SetText(zoom);

	float currentZoom = CESettings.videoZoom;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(settingText);

	if(!SettingWindow("Screen Zoom",w))
		CESettings.videoZoom = currentZoom; // undo changes

	delete(w);
	delete(settingText);
}

static void ScreenPositionWindowUpdate(void * ptr, int x, int y)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		CESettings.videoXshift += x;
		CESettings.videoYshift += y;

		char shift[10];
		sprintf(shift, "%i, %i", CESettings.videoXshift, CESettings.videoYshift);
		settingText->SetText(shift);
		b->ResetState();
	}
}

static void ScreenPositionWindowLeftClick(void * ptr) { ScreenPositionWindowUpdate(ptr, -1, 0); }
static void ScreenPositionWindowRightClick(void * ptr) { ScreenPositionWindowUpdate(ptr, +1, 0); }
static void ScreenPositionWindowUpClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, -1); }
static void ScreenPositionWindowDownClick(void * ptr) { ScreenPositionWindowUpdate(ptr, 0, +1); }

static void ScreenPositionWindow()
{
	GuiWindow * w = new GuiWindow(150,150);
	w->SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	w->SetPosition(0, -10);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigLeft;
	trigLeft.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);

	GuiTrigger trigRight;
	trigRight.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);

	GuiTrigger trigUp;
	trigUp.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP, PAD_BUTTON_UP);

	GuiTrigger trigDown;
	trigDown.SetButtonOnlyInFocusTrigger(-1, WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN, PAD_BUTTON_DOWN);

	GuiImageData arrowLeft(button_arrow_left_png);
	GuiImage arrowLeftImg(&arrowLeft);
	GuiImageData arrowLeftOver(button_arrow_left_over_png);
	GuiImage arrowLeftOverImg(&arrowLeftOver);
	GuiButton arrowLeftBtn(arrowLeft.GetWidth(), arrowLeft.GetHeight());
	arrowLeftBtn.SetImage(&arrowLeftImg);
	arrowLeftBtn.SetImageOver(&arrowLeftOverImg);
	arrowLeftBtn.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	arrowLeftBtn.SetTrigger(0, &trigA);
	arrowLeftBtn.SetTrigger(1, &trigLeft);
	arrowLeftBtn.SetSelectable(false);
	arrowLeftBtn.SetUpdateCallback(ScreenPositionWindowLeftClick);

	GuiImageData arrowRight(button_arrow_right_png);
	GuiImage arrowRightImg(&arrowRight);
	GuiImageData arrowRightOver(button_arrow_right_over_png);
	GuiImage arrowRightOverImg(&arrowRightOver);
	GuiButton arrowRightBtn(arrowRight.GetWidth(), arrowRight.GetHeight());
	arrowRightBtn.SetImage(&arrowRightImg);
	arrowRightBtn.SetImageOver(&arrowRightOverImg);
	arrowRightBtn.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	arrowRightBtn.SetTrigger(0, &trigA);
	arrowRightBtn.SetTrigger(1, &trigRight);
	arrowRightBtn.SetSelectable(false);
	arrowRightBtn.SetUpdateCallback(ScreenPositionWindowRightClick);

	GuiImageData arrowUp(button_arrow_up_png);
	GuiImage arrowUpImg(&arrowUp);
	GuiImageData arrowUpOver(button_arrow_up_over_png);
	GuiImage arrowUpOverImg(&arrowUpOver);
	GuiButton arrowUpBtn(arrowUp.GetWidth(), arrowUp.GetHeight());
	arrowUpBtn.SetImage(&arrowUpImg);
	arrowUpBtn.SetImageOver(&arrowUpOverImg);
	arrowUpBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	arrowUpBtn.SetTrigger(0, &trigA);
	arrowUpBtn.SetTrigger(1, &trigUp);
	arrowUpBtn.SetSelectable(false);
	arrowUpBtn.SetUpdateCallback(ScreenPositionWindowUpClick);

	GuiImageData arrowDown(button_arrow_down_png);
	GuiImage arrowDownImg(&arrowDown);
	GuiImageData arrowDownOver(button_arrow_down_over_png);
	GuiImage arrowDownOverImg(&arrowDownOver);
	GuiButton arrowDownBtn(arrowDown.GetWidth(), arrowDown.GetHeight());
	arrowDownBtn.SetImage(&arrowDownImg);
	arrowDownBtn.SetImageOver(&arrowDownOverImg);
	arrowDownBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	arrowDownBtn.SetTrigger(0, &trigA);
	arrowDownBtn.SetTrigger(1, &trigDown);
	arrowDownBtn.SetSelectable(false);
	arrowDownBtn.SetUpdateCallback(ScreenPositionWindowDownClick);

	GuiImageData screenPosition(screen_position_png);
	GuiImage screenPositionImg(&screenPosition);
	screenPositionImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	settingText = new GuiText(NULL, 22, (GXColor){0, 0, 0, 255});
	char shift[10];
	sprintf(shift, "%i, %i", CESettings.videoXshift, CESettings.videoYshift);
	settingText->SetText(shift);

	int currentX = CESettings.videoXshift;
	int currentY = CESettings.videoYshift;

	w->Append(&arrowLeftBtn);
	w->Append(&arrowRightBtn);
	w->Append(&arrowUpBtn);
	w->Append(&arrowDownBtn);
	w->Append(&screenPositionImg);
	w->Append(settingText);

	if(!SettingWindow("Screen Position",w))
	{
		CESettings.videoXshift = currentX; // undo changes
		CESettings.videoYshift = currentY;
	}

	delete(w);
	delete(settingText);
}

static void MenuSettingsVideo()
{
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Frame Dropping");
	sprintf(options.name[i++], "Aspect Ratio");
	sprintf(options.name[i++], "Zoom");
	sprintf(options.name[i++], "Position");

	options.length = i;
		
	for(i=0; i < options.length; i++)
	{
		options.value[i][0] = 0;
		options.icon[i] = 0;
	}

	GuiText titleTxt("Settings - Video", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol2Position(220);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_VIDEO && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				CESettings.frameDropping++;
				if (CESettings.frameDropping > 2)
					CESettings.frameDropping = 0;
				break;

			case 1:
				CESettings.aspectRatio++;
				if (CESettings.aspectRatio > 3)
					CESettings.aspectRatio = 0;
				break;

			case 2:
				ScreenZoomWindow();
				break;

			case 3:
				ScreenPositionWindow();
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			switch(CESettings.frameDropping)
			{
				case FRAMEDROPPING_AUTO:
					sprintf (options.value[0], "Auto"); break;
				case FRAMEDROPPING_ALWAYS:
					sprintf (options.value[0], "Always"); break;
				case FRAMEDROPPING_DISABLED:
					sprintf (options.value[0], "Disabled"); break;
			}

			switch(CESettings.aspectRatio)
			{
				case ASPECT_AUTO:
					sprintf (options.value[1], "Auto"); break;
				case ASPECT_16_9:
					sprintf (options.value[1], "16:9"); break;
				case ASPECT_4_3:
					sprintf (options.value[1], "4:3"); break;
				case ASPECT_235_1:
					sprintf (options.value[1], "2.35:1"); break;
			}

			sprintf (options.value[2], "%.2f%%", CESettings.videoZoom*100);
			sprintf (options.value[3], "%d, %d", CESettings.videoXshift, CESettings.videoYshift);

			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void MenuSettingsAudio()
{
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Volume");
	sprintf(options.name[i++], "Delay (ms)");

	options.length = i;
		
	for(i=0; i < options.length; i++)
	{
		options.value[i][0] = 0;
		options.icon[i] = 0;
	}

	GuiText titleTxt("Settings - Audio", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol2Position(220);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_AUDIO && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				CESettings.volume += 10;
				if(CESettings.volume > 100)
					CESettings.volume = 0;
				break;

			case 1:
				CESettings.audioDelay += 100;
				if (CESettings.audioDelay > 1000)
					CESettings.audioDelay = 0;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			sprintf (options.value[0], "%d%%", CESettings.volume);
			sprintf (options.value[1], "%d ms", CESettings.audioDelay);

			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void MenuSettingsSubtitles()
{
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;

	sprintf(options.name[i++], "Delay");
	sprintf(options.name[i++], "Position");
	sprintf(options.name[i++], "Size");
	sprintf(options.name[i++], "Transparency");
	sprintf(options.name[i++], "Color");

	options.length = i;
		
	for(i=0; i < options.length; i++)
	{
		options.value[i][0] = 0;
		options.icon[i] = 0;
	}

	GuiText titleTxt("Settings - Subtitles", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(460, 248, &options);
	optionBrowser.SetPosition(30, 120);
	optionBrowser.SetCol2Position(220);
	optionBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS_SUBTITLES && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				CESettings.subtitleDelay += 100;
				if (CESettings.subtitleDelay > 1000)
					CESettings.subtitleDelay = 0;
				break;

			case 1:
				CESettings.subtitlePosition += 10;
				if (CESettings.subtitlePosition > 100)
					CESettings.subtitlePosition = 100;
				break;

			case 2:
				CESettings.subtitleSize += 4;
				if (CESettings.subtitleSize > 80)
					CESettings.subtitleSize = 10;
				break;

			case 3:
				CESettings.subtitleAlpha += 20;
				if(CESettings.subtitleAlpha > 255)
					CESettings.subtitleAlpha = 0;
				break;

			case 4:
				CESettings.subtitleColor++;
				if(CESettings.subtitleColor > FONTCOLOR_GRAY)
					CESettings.subtitleColor = 0;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;
			sprintf(options.value[0], "%d ms", CESettings.subtitleDelay);
			sprintf(options.value[1], "%d", CESettings.subtitlePosition);
			sprintf(options.value[2], "%d", CESettings.subtitleSize);
			sprintf(options.value[3], "%.2f%%", CESettings.subtitleAlpha/255.0*100);
			
			switch(CESettings.subtitleColor)
			{
				case FONTCOLOR_WHITE:
					sprintf(options.value[4], "White"); break;
				case FONTCOLOR_BLACK:
					sprintf(options.value[4], "Black"); break;
				case FONTCOLOR_GRAY:
					sprintf(options.value[4], "Gray"); break;
			}

			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_SETTINGS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

/****************************************************************************
 * MenuSettings
 ***************************************************************************/
static void MenuSettings()
{
	int ret;
	int i = 0;
	int selected = -1;

	MenuItemList items;
	sprintf(items.name[i], "General");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Cache");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Network");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Video");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Audio");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Subtitles");
	items.img[i] = NULL; i++;
	items.length = i;

	GuiText titleTxt("Settings", 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(30, 80);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiText backBtnTxt("Go Back", 24, (GXColor){255, 255, 255, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(30, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiMenuBrowser itemBrowser(300, 400, &items);
	itemBrowser.SetPosition(30, 120);
	itemBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	mainWindow->Append(&itemBrowser);
	mainWindow->Append(&backBtn);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_SETTINGS && !guiShutdown)
	{
		usleep(THREAD_SLEEP);

		if(selected != itemBrowser.GetSelectedItem())
		{
			selected = itemBrowser.GetSelectedItem();
		}

		ret = itemBrowser.GetClickedItem();

		switch (ret)
		{
			case 0:
				currentMenu = MENU_SETTINGS_GENERAL;
				break;

			case 1:
				currentMenu = MENU_SETTINGS_CACHE;
				break;

			case 2:
				currentMenu = MENU_SETTINGS_NETWORK;
				break;

			case 3:
				currentMenu = MENU_SETTINGS_VIDEO;
				break;

			case 4:
				currentMenu = MENU_SETTINGS_AUDIO;
				break;

			case 5:
				currentMenu = MENU_SETTINGS_SUBTITLES;
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
			ChangeMenu(lastMenu);
	}

	HaltGui();
	mainWindow->Remove(&itemBrowser);
	mainWindow->Remove(&titleTxt);
}

static void BackToMplayerCallback(void * ptr)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		b->ResetState();
		LoadMPlayer(); // signal MPlayer to resume
	}
}

/****************************************************************************
 * Menu
 ***************************************************************************/
void WiiMenu()
{
	menuMode = 0; // switch to normal GUI mode
	guiShutdown = false;
	selectLoadedFile = true;

	if(pointer[0] == NULL)
	{
		pointer[0] = new GuiImageData(player1_point_png);
		pointer[1] = new GuiImageData(player2_point_png);
		pointer[2] = new GuiImageData(player3_point_png);
		pointer[3] = new GuiImageData(player4_point_png);
	}

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	mainWindow = new GuiWindow(screenwidth, screenheight);
	
	GuiImage bgLeft(500, screenheight, (GXColor){0, 0, 0, 255});
	bgLeft.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	GuiImage bgRight(140, screenheight, (GXColor){155, 155, 155, 255});
	bgRight.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	
	GuiImageData btnNav(nav_button_png);
	GuiImageData btnNavOver(nav_button_png);
	
	GuiText mplayerBtnTxt("MPlayer", 18, (GXColor){255, 255, 255, 255});
	GuiImage mplayerBtnImg(&btnNav);
	GuiImage mplayerBtnImgOver(&btnNavOver);
	mplayerBtn = new GuiButton(btnNav.GetWidth(), btnNav.GetHeight());
	mplayerBtn->SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	mplayerBtn->SetPosition(-30, 20);
	mplayerBtn->SetLabel(&mplayerBtnTxt);
	mplayerBtn->SetImage(&mplayerBtnImg);
	mplayerBtn->SetImageOver(&mplayerBtnImgOver);
	mplayerBtn->SetTrigger(&trigA);
	mplayerBtn->SetEffectGrow();
	mplayerBtn->SetUpdateCallback(BackToMplayerCallback);
	
	mainWindow->Append(&bgLeft);
	
	if(videoScreenshot)
	{
		videoImg = new GuiImage(videoScreenshot, screenwidth, screenheight);
		mainWindow->Append(videoImg);
		mainWindow->Append(mplayerBtn);
	}
	else
	{
		mainWindow->Append(&bgRight);
	}

	GuiImageData btnConfig(config_button_png);

	GuiText videoBtnTxt("Videos", 18, (GXColor){255, 255, 255, 255});
	GuiImage videoBtnImg(&btnNav);
	GuiImage videoBtnImgOver(&btnNavOver);
	videoBtn = new GuiButton(btnNav.GetWidth(), btnNav.GetHeight());
	videoBtn->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	videoBtn->SetPosition(30, 30);
	videoBtn->SetLabel(&videoBtnTxt);
	//videoBtn->SetImage(&videoBtnImg);
	//videoBtn->SetImageOver(&videoBtnImgOver);
	videoBtn->SetTrigger(&trigA);
	videoBtn->SetEffectGrow();
	videoBtn->SetUpdateCallback(ChangeMenuVideos);

	GuiText musicBtnTxt("Music", 18, (GXColor){255, 255, 255, 255});
	GuiImage musicBtnImg(&btnNav);
	GuiImage musicBtnImgOver(&btnNavOver);
	musicBtn = new GuiButton(btnNav.GetWidth(), btnNav.GetHeight());
	musicBtn->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	musicBtn->SetPosition(160, 30);
	musicBtn->SetLabel(&musicBtnTxt);
	//musicBtn->SetImage(&musicBtnImg);
	//musicBtn->SetImageOver(&musicBtnImgOver);
	musicBtn->SetTrigger(&trigA);
	musicBtn->SetEffectGrow();
	musicBtn->SetUpdateCallback(ChangeMenuMusic);

	GuiText dvdBtnTxt("DVD", 18, (GXColor){255, 255, 255, 255});
	GuiImage dvdBtnImg(&btnNav);
	GuiImage dvdBtnImgOver(&btnNavOver);
	dvdBtn = new GuiButton(btnNav.GetWidth(), btnNav.GetHeight());
	dvdBtn->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	dvdBtn->SetPosition(240, 30);
	dvdBtn->SetLabel(&dvdBtnTxt);
	//dvdBtn->SetImage(&dvdBtnImg);
	//dvdBtn->SetImageOver(&dvdBtnImgOver);
	dvdBtn->SetTrigger(&trigA);
	dvdBtn->SetEffectGrow();
	dvdBtn->SetUpdateCallback(ChangeMenuDVD);

	GuiText onlineBtnTxt("Online Media", 18, (GXColor){255, 255, 255, 255});
	GuiImage onlineBtnImg(&btnNav);
	GuiImage onlineBtnImgOver(&btnNavOver);
	onlineBtn = new GuiButton(btnNav.GetWidth(), btnNav.GetHeight());
	onlineBtn->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	onlineBtn->SetPosition(340, 30);
	onlineBtn->SetLabel(&onlineBtnTxt);
	//onlineBtn->SetImage(&onlineBtnImg);
	//onlineBtn->SetImageOver(&onlineBtnImgOver);
	onlineBtn->SetTrigger(&trigA);
	onlineBtn->SetEffectGrow();
	onlineBtn->SetUpdateCallback(ChangeMenuOnline);

	GuiImage configBtnImg(&btnConfig);
	configBtn = new GuiButton(btnConfig.GetWidth(), btnConfig.GetHeight());
	configBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	configBtn->SetPosition(-150, -20);
	configBtn->SetImage(&configBtnImg);
	configBtn->SetTrigger(&trigA);
	configBtn->SetEffectGrow();
	configBtn->SetUpdateCallback(ChangeMenuSettings);

	mainWindow->Append(videoBtn);
	mainWindow->Append(musicBtn);
	mainWindow->Append(dvdBtn);
	mainWindow->Append(onlineBtn);
	mainWindow->Append(configBtn);

	GuiImageData logo(logo_png);
	GuiImage logoBtnImg(&logo);
	logoBtn = new GuiButton(logo.GetWidth(), logo.GetHeight());
	logoBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	logoBtn->SetPosition(-10, -20);
	logoBtn->SetImage(&logoBtnImg);
	logoBtn->SetTrigger(&trigA);
	logoBtn->SetUpdateCallback(DisplayCredits);
	mainWindow->Append(logoBtn);

	ResumeGui();

	// Load settings
	if(!LoadSettings())
		SaveSettings(SILENT);

	while(!guiShutdown)
	{
		switch (currentMenu)
		{
			case MENU_BROWSE_VIDEOS:
			case MENU_BROWSE_MUSIC:
			case MENU_BROWSE_ONLINEMEDIA:
				MenuBrowse(currentMenu);
				break;
			case MENU_DVD:
				MenuDVD();
				break;
			case MENU_SETTINGS:
				MenuSettings();
				break;
			case MENU_SETTINGS_GENERAL:
				MenuSettingsGeneral();
				break;
			case MENU_SETTINGS_CACHE:
				MenuSettingsCache();
				break;
			case MENU_SETTINGS_NETWORK:
				MenuSettingsNetwork();
				break;
			case MENU_SETTINGS_NETWORK_SMB:
				MenuSettingsNetworkSMB();
				break;
			case MENU_SETTINGS_NETWORK_FTP:
				MenuSettingsNetworkFTP();
				break;
			case MENU_SETTINGS_VIDEO:
				MenuSettingsVideo();
				break;
			case MENU_SETTINGS_AUDIO:
				MenuSettingsAudio();
				break;
			case MENU_SETTINGS_SUBTITLES:
				MenuSettingsSubtitles();
				break;
			default: // unrecognized menu
				MenuBrowse(MENU_BROWSE_VIDEOS);
				break;
		}
		
		usleep(THREAD_SLEEP);
	}

	ShutoffRumble();
	CancelAction();
	HaltGui();

	delete mainWindow;
	mainWindow = NULL;

	delete videoBtn;
	videoBtn = NULL;
	delete musicBtn;
	musicBtn = NULL;
	delete dvdBtn;
	dvdBtn = NULL;
	delete onlineBtn;
	onlineBtn = NULL;
	delete configBtn;
	configBtn = NULL;
	delete logoBtn;
	logoBtn = NULL;
	delete mplayerBtn;
	mplayerBtn = NULL;

	if(videoImg)
	{
		delete videoImg;
		videoImg = NULL;
	}

	if(videoScreenshot)
	{
		free(videoScreenshot);
		videoScreenshot = NULL;
	}
}

/****************************************************************************
 * MPlayer Menu
 ***************************************************************************/
void MPlayerMenu()
{
	menuMode = 1; // switch to MPlayer GUI mode
	
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	mainWindow = new GuiWindow(screenwidth, screenheight);

	GuiImage bgBottom(screenwidth, 140, (GXColor){155, 155, 155, 155});
	bgBottom.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	
	GuiImageData btnNav(nav_button_png);
	GuiImageData btnNavOver(nav_button_png);
	
	GuiText playBtnTxt("Play", 18, (GXColor){255, 255, 255, 255});
	GuiImage playBtnImg(&btnNav);
	GuiImage playBtnImgOver(&btnNavOver);
	GuiButton playBtn(btnNav.GetWidth(), btnNav.GetHeight());
	playBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	playBtn.SetPosition(30, -40);
	playBtn.SetLabel(&playBtnTxt);
	playBtn.SetImage(&playBtnImg);
	playBtn.SetImageOver(&playBtnImgOver);
	playBtn.SetTrigger(&trigA);
	playBtn.SetEffectGrow();
	
	mainWindow->Append(&bgBottom);
	mainWindow->Append(&playBtn);

	ResumeGui();

	while(!controlledbygui)
	{
		usleep(THREAD_SLEEP);
	}

	ShutoffRumble();
	CancelAction();
	HaltGui();

	delete mainWindow;
	mainWindow = NULL;
}
