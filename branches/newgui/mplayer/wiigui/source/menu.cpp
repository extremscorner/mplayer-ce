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
#include "fileop.h"
#include "input.h"
#include "networkop.h"
#include "filebrowser.h"
#include "filelist.h"

#define THREAD_SLEEP 100

static GuiImageData * pointer[4];
static GuiImage * videoImg = NULL;
static GuiButton * videoBtn = NULL;
static GuiButton * musicBtn = NULL;
static GuiButton * dvdBtn = NULL;
static GuiButton * onlineBtn = NULL;
static GuiButton * logoBtn = NULL;
static GuiWindow * mainWindow = NULL;

static int currentMenu = MENU_BROWSE;

static lwp_t guithread = LWP_THREAD_NULL;
static lwp_t progressthread = LWP_THREAD_NULL;
static bool guiHalt = true;
static bool shutdownGui = true;
static int showProgress = 0;

static char progressTitle[100];
static char progressMsg[200];
static int progressDone = 0;
static int progressTotal = 0;

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/

static void *
UpdateGUI (void *arg)
{
	while(1)
	{
		if(guiHalt)
		{
			break;
		}
		else
		{
			mainWindow->Draw();

			#ifdef HW_RVL
			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad.ir.valid)
					Menu_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48,
						96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, 1, 1, 255);
				DoRumble(i);
			}
			#endif

			Menu_Render();

			for(int i=0; i < 4; i++)
				mainWindow->Update(&userInput[i]);

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
		LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
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

void
HaltGui()
{
	guiHalt = true;

	if(guithread == LWP_THREAD_NULL)
		return;

	// wait for thread to finish
	LWP_JoinThread(guithread, NULL);
	guithread = LWP_THREAD_NULL;
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
 * WindowCredits
 * Display credits, legal copyright and licence
 *
 * THIS MUST NOT BE REMOVED OR DISABLED IN ANY DERIVATIVE WORK
 ***************************************************************************/
static void WindowCredits(void * ptr)
{
	if(logoBtn->GetState() != STATE_CLICKED)
		return;

	logoBtn->ResetState();

	bool exit = false;
	int i = 0;
	int y = 20;

	GuiWindow creditsWindow(screenwidth,screenheight);
	GuiWindow creditsWindowBox(580,448);
	creditsWindowBox.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiImageData creditsBox(credits_box_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	creditsWindowBox.Append(&creditsBoxImg);

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
		creditsWindowBox.Append(txt[i]);

	creditsWindow.Append(&creditsWindowBox);

	while(!exit)
	{
		creditsWindow.Draw();

		for(i=3; i >= 0; i--)
		{
			#ifdef HW_RVL
			if(userInput[i].wpad.ir.valid)
				Menu_DrawImg(userInput[i].wpad.ir.x-48, userInput[i].wpad.ir.y-48,
					96, 96, pointer[i]->GetImage(), userInput[i].wpad.ir.angle, 1, 1, 255);
			DoRumble(i);
			#endif
		}

		Menu_Render();

		for(i=0; i < 4; i++)
		{
			if(userInput[i].wpad.btns_d || userInput[i].pad.btns_d)
				exit = true;
		}
		usleep(THREAD_SLEEP);
	}

	// clear buttons pressed
	for(i=0; i < 4; i++)
	{
		userInput[i].wpad.btns_d = 0;
		userInput[i].pad.btns_d = 0;
	}

	for(i=0; i < numEntries; i++)
		delete txt[i];
}

static void ChangeMenu(void * ptr, int menu)
{
	GuiButton * b = (GuiButton *)ptr;
	if(b->GetState() == STATE_CLICKED)
	{
		currentMenu = menu;
		b->ResetState();
	}
}
static void ChangeMenuVideos(void * ptr) { ChangeMenu(ptr, MENU_BROWSE); }
static void ChangeMenuMusic(void * ptr) { ChangeMenu(ptr, MENU_BROWSE); }
static void ChangeMenuDVD(void * ptr) {	ChangeMenu(ptr, MENU_DVD); }
static void ChangeMenuOnline(void * ptr) { ChangeMenu(ptr, MENU_ONLINEMEDIA); }

/****************************************************************************
 * MenuBrowse
 ***************************************************************************/

static void MenuBrowse()
{
	ShutoffRumble();

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
			currentMenu = MENU_OPTIONS;
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

	while(currentMenu == MENU_BROWSE)
	{
		usleep(THREAD_SLEEP);

		// update file browser based on arrow buttons
		// request shutdownGui if A button pressed on a file
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
					sprintf(loadedFile, "%s%s", browser.dir, browserList[browser.selIndex].filename);

					ShowAction("Loading...");

					// signal MPlayer to load
					loadMPlayer();

					// wait until MPlayer is ready to take control
					while(!guiHalt)
						usleep(THREAD_SLEEP);

					CancelAction();
					shutdownGui = true;
					goto done;
				}
			}
		}
	}
done:
	HaltGui();
	mainWindow->Remove(&fileBrowser);
}

static void MenuOnlineMedia()
{
	currentMenu = MENU_BROWSE;
}

static void MenuDVD()
{
	currentMenu = MENU_BROWSE;
}

static void MenuOptionsVideo()
{
	int ret;
	int i = 0;
	OptionList options;

	sprintf(options.name[i++], "Frame Dropping");
	sprintf(options.name[i++], "Aspect Ratio");
	options.length = i;

	GuiText titleTxt("Options - Video", 36, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(340,50);

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

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetCol2Position(200);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_OPTIONS_VIDEO)
	{
		usleep(THREAD_SLEEP);

		switch(CESettings.frameDropping)
		{
			case 0:
				sprintf (options.value[0], "Enabled"); break;
			case 1:
				sprintf (options.value[0], "Hard"); break;
			case 2:
				sprintf (options.value[0], "Disabled"); break;
		}

		switch(CESettings.aspectRatio)
		{
			case 0:
				sprintf (options.value[1], "Original"); break;
			case 1:
				sprintf (options.value[1], "16:9"); break;
			case 2:
				sprintf (options.value[1], "4:3"); break;
			case 3:
				sprintf (options.value[1], "2.35:1"); break;
		}

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
		}
		if(backBtn.GetState() == STATE_CLICKED)
		{
			currentMenu = MENU_OPTIONS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
}

static void MenuOptionsAudio()
{
	currentMenu = MENU_OPTIONS;
}

static void MenuOptionsSubtitles()
{
	currentMenu = MENU_OPTIONS;
}

static void MenuOptionsMenu()
{
	currentMenu = MENU_OPTIONS;
}

/****************************************************************************
 * MenuOptions
 ***************************************************************************/
static void MenuOptions()
{
	int ret;
	int i = 0;
	int selected = -1;

	MenuItemList items;
	sprintf(items.name[i], "Video");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Audio");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Subtitles");
	items.img[i] = NULL; i++;
	sprintf(items.name[i], "Menu");
	items.img[i] = NULL; i++;
	items.length = i;

	GuiText titleTxt("Options", 36, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(340,50);

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
	itemBrowser.SetPosition(280, 120);
	itemBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	mainWindow->Append(&itemBrowser);
	mainWindow->Append(&backBtn);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(currentMenu == MENU_OPTIONS)
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
				currentMenu = MENU_OPTIONS_VIDEO;
				break;

			case 1:
				currentMenu = MENU_OPTIONS_AUDIO;
				break;

			case 2:
				currentMenu = MENU_OPTIONS_SUBTITLES;
				break;

			case 3:
				currentMenu = MENU_OPTIONS_MENU;
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
			currentMenu = MENU_BROWSE;
	}

	HaltGui();
	mainWindow->Remove(&itemBrowser);
	mainWindow->Remove(&titleTxt);
}

/****************************************************************************
 * Menu
 ***************************************************************************/
void WiiMenu()
{
	shutdownGui = false;

	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	mainWindow = new GuiWindow(screenwidth, screenheight);

	GuiImageData btnNav(nav_button_png);
	GuiImageData btnNavOver(nav_button_png);

	GuiText videoBtnTxt("Videos & Pictures", 18, (GXColor){255, 255, 255, 255});
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

	mainWindow->Append(videoBtn);
	mainWindow->Append(musicBtn);
	mainWindow->Append(dvdBtn);
	mainWindow->Append(onlineBtn);

	GuiImage bg(140, screenheight, (GXColor){155, 155, 155, 255});
	bg.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	mainWindow->Append(&bg);

	if(videoScreenshot)
	{
		videoImg = new GuiImage(videoScreenshot, screenwidth, screenheight);
		mainWindow->Append(videoImg);
	}

	GuiImageData logo(logo_png);
	GuiImage logoBtnImg(&logo);
	logoBtn = new GuiButton(logo.GetWidth(), logo.GetHeight());
	logoBtn->SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	logoBtn->SetPosition(-10, -20);
	logoBtn->SetImage(&logoBtnImg);
	logoBtn->SetTrigger(&trigA);
	logoBtn->SetUpdateCallback(WindowCredits);
	mainWindow->Append(logoBtn);

	ResumeGui();

	while(!shutdownGui)
	{
		switch (currentMenu)
		{
			case MENU_BROWSE:
				MenuBrowse();
				break;
			case MENU_DVD:
				MenuDVD();
				break;
			case MENU_ONLINEMEDIA:
				MenuOnlineMedia();
				break;
			case MENU_OPTIONS:
				MenuOptions();
				break;
			case MENU_OPTIONS_VIDEO:
				MenuOptionsVideo();
				break;
			case MENU_OPTIONS_AUDIO:
				MenuOptionsAudio();
				break;
			case MENU_OPTIONS_SUBTITLES:
				MenuOptionsSubtitles();
				break;
			case MENU_OPTIONS_MENU:
				MenuOptionsMenu();
				break;
			default: // unrecognized menu
				MenuBrowse();
				break;
		}
		usleep(THREAD_SLEEP);
	}

	ShutoffRumble();
	CancelAction();
	HaltGui();

	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];

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
	delete logoBtn;
	logoBtn = NULL;

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
