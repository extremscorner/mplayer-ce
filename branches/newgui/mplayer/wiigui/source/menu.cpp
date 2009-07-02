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
static GuiImage * bgImg = NULL;
static GuiImage * bgImgTop = NULL;
static GuiImage * bgImgBottom = NULL;
static GuiButton * btnLogo = NULL;
static GuiWindow * mainWindow = NULL;

static int lastMenu = MENU_NONE;

static lwp_t guithread = LWP_THREAD_NULL;
static lwp_t progressthread = LWP_THREAD_NULL;
static bool guiHalt = true;
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

	GuiText titleTxt(title, 26, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);
	GuiText msgTxt(msg, 22, (GXColor){255, 255, 255, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetMaxWidth(430);

	GuiText btn1Txt(btn1Label, 22, (GXColor){255, 255, 255, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
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
	btn2.SetPosition(-20, -25);
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

	GuiText titleTxt(title, 26, (GXColor){70, 70, 10, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,14);
	GuiText msgTxt(msg, 26, (GXColor){255, 255, 255, 255});
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
 * MenuHome
 *
 * Menu displayed when returning to the menu from in-video.
 ***************************************************************************/
static int MenuHome()
{
	int menu = MENU_NONE;

	GuiText titleTxt("MPlayer CE", 24, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiImageData btnCloseOutline(button_small_png);
	GuiImageData btnCloseOutlineOver(button_small_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiImageData battery(battery_png);
	GuiImageData batteryRed(battery_red_png);
	GuiImageData batteryBar(battery_bar_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText mainmenuBtnTxt("Main Menu", 24, (GXColor){0, 0, 0, 255});
	GuiImage mainmenuBtnImg(&btnLargeOutline);
	GuiImage mainmenuBtnImgOver(&btnLargeOutlineOver);
	GuiButton mainmenuBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	mainmenuBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	mainmenuBtn.SetPosition(-125, 120);
	mainmenuBtn.SetLabel(&mainmenuBtnTxt);
	mainmenuBtn.SetImage(&mainmenuBtnImg);
	mainmenuBtn.SetImageOver(&mainmenuBtnImgOver);
	mainmenuBtn.SetTrigger(&trigA);
	mainmenuBtn.SetEffectGrow();

	GuiText exitBtnTxt("Exit", 24, (GXColor){0, 0, 0, 255});
	GuiImage exitBtnImg(&btnLargeOutline);
	GuiImage exitBtnImgOver(&btnLargeOutlineOver);
	GuiButton exitBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	exitBtn.SetPosition(125, 120);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetEffectGrow();

	GuiText closeBtnTxt("Close", 22, (GXColor){0, 0, 0, 255});
	GuiImage closeBtnImg(&btnCloseOutline);
	GuiImage closeBtnImgOver(&btnCloseOutlineOver);
	GuiButton closeBtn(btnCloseOutline.GetWidth(), btnCloseOutline.GetHeight());
	closeBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	closeBtn.SetPosition(-50, 35);
	closeBtn.SetLabel(&closeBtnTxt);
	closeBtn.SetImage(&closeBtnImg);
	closeBtn.SetImageOver(&closeBtnImgOver);
	closeBtn.SetTrigger(&trigA);
	closeBtn.SetTrigger(&trigHome);
	closeBtn.SetEffectGrow();

	int i, level;
	char txt[3];
	GuiText * batteryTxt[4];
	GuiImage * batteryImg[4];
	GuiImage * batteryBarImg[4];
	GuiButton * batteryBtn[4];

	for(i=0; i < 4; i++)
	{
		if(i == 0)
			sprintf(txt, "P %d", i+1);
		else
			sprintf(txt, "P%d", i+1);

		batteryTxt[i] = new GuiText(txt, 22, (GXColor){255, 255, 255, 255});
		batteryTxt[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i] = new GuiImage(&battery);
		batteryImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i]->SetPosition(30, 0);
		batteryBarImg[i] = new GuiImage(&batteryBar);
		batteryBarImg[i]->SetTile(0);
		batteryBarImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryBarImg[i]->SetPosition(34, 0);

		batteryBtn[i] = new GuiButton(70, 20);
		batteryBtn[i]->SetLabel(batteryTxt[i]);
		batteryBtn[i]->SetImage(batteryImg[i]);
		batteryBtn[i]->SetIcon(batteryBarImg[i]);
		batteryBtn[i]->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		batteryBtn[i]->SetRumble(false);
		batteryBtn[i]->SetAlpha(150);
	}

	batteryBtn[0]->SetPosition(45, -65);
	batteryBtn[1]->SetPosition(135, -65);
	batteryBtn[2]->SetPosition(45, -40);
	batteryBtn[3]->SetPosition(135, -40);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&mainmenuBtn);
	w.Append(&exitBtn);
	w.Append(batteryBtn[0]);
	w.Append(batteryBtn[1]);
	w.Append(batteryBtn[2]);
	w.Append(batteryBtn[3]);

	w.Append(&closeBtn);

	mainWindow->Append(&w);

	if(lastMenu == MENU_NONE)
	{
		bgImgTop->SetVisible(true);
		bgImgBottom->SetVisible(true);

		bgImgTop->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 35);
		mainmenuBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		bgImgBottom->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);

		batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);
		batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 35);

		w.SetEffect(EFFECT_FADE, 15);
	}

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		for(i=0; i < 4; i++)
		{
			if(WPAD_Probe(i, NULL) == WPAD_ERR_NONE) // controller connected
			{
				level = (userInput[i].wpad.battery_level / 100.0) * 4;
				if(level > 4) level = 4;
				batteryBarImg[i]->SetTile(level);

				if(level == 0)
					batteryImg[i]->SetImage(&batteryRed);
				else
					batteryImg[i]->SetImage(&battery);

				batteryBtn[i]->SetAlpha(255);
			}
			else // controller not connected
			{
				batteryBarImg[i]->SetTile(0);
				batteryImg[i]->SetImage(&battery);
				batteryBtn[i]->SetAlpha(150);
			}
		}

		if(exitBtn.GetState() == STATE_CLICKED)
		{
			ExitRequested = 1;
		}
		else if(mainmenuBtn.GetState() == STATE_CLICKED)
		{
			if(videoImg)
			{
				mainWindow->Remove(videoImg);
				delete videoImg;
				videoImg = NULL;
			}
			if(videoScreenshot)
			{
				free(videoScreenshot);
				videoScreenshot = NULL;
			}
			menu = MENU_MAIN;
			loadedFile[0] = 0;
		}
		else if(closeBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;

			bgImgTop->SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 15);
			mainmenuBtn.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			bgImgBottom->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);

			batteryBtn[0]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[1]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[2]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);
			batteryBtn[3]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 15);

			w.SetEffect(EFFECT_FADE, -15);
			usleep(350000); // wait for effects to finish
		}
	}

	HaltGui();

	for(i=0; i < 4; i++)
	{
		delete batteryTxt[i];
		delete batteryImg[i];
		delete batteryBarImg[i];
		delete batteryBtn[i];
	}

	mainWindow->Remove(&w);
	return menu;
}

/****************************************************************************
 * MenuBrowseDevice
 ***************************************************************************/

static int MenuBrowseDevice()
{
	char deviceName[100];
	char title[100];

	ShutoffRumble();

	// populate initial directory listing
	if(BrowseDevice() <= 0)
	{
		int choice = WindowPrompt(
		"Error",
		"Unable to display files on selected load device.",
		"Retry",
		"Check Settings");

		if(choice)
			return MENU_BROWSE_DEVICE;
		else
			return MENU_OPTIONS;
	}

	int menu = MENU_NONE;

	GuiImageData browseSmall(browse_small_png);
	GuiImage browseSmallImg(&browseSmall);
	browseSmallImg.SetPosition(30,30);

	switch(currentDevice)
	{
		case DEVICE_SD:
			sprintf(deviceName, "SD Card");
			break;
		case DEVICE_USB:
			sprintf(deviceName, "USB Mass Storage");
			break;
		case DEVICE_DVD:
			sprintf(deviceName, "Data DVD");
			break;
		case DEVICE_SMB:
			snprintf(deviceName, 100, "%s (Network)", smbConf[currentDeviceNum].share);
			break;
	}
	sprintf(title, "Browse Files - %s", deviceName);

	GuiText titleTxt(title, 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(100,50);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiFileBrowser fileBrowser(580, 300);
	fileBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	fileBrowser.SetPosition(0, 100);

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

	GuiWindow buttonWindow(screenwidth, screenheight);
	buttonWindow.Append(&backBtn);

	HaltGui();
	mainWindow->Append(&titleTxt);
	mainWindow->Append(&browseSmallImg);
	mainWindow->Append(&fileBrowser);
	mainWindow->Append(&buttonWindow);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		// update file browser based on arrow buttons
		// set MENU_EXIT if A button pressed on a file
		for(int i=0; i<FILES_PAGESIZE; i++)
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
						menu = MENU_BROWSE_DEVICE;
						break;
					}
				}
				else
				{
					sprintf(loadedFile, "%s%s%s", rootdir, browser.dir, browserList[browser.selIndex].filename);
					menu = MENU_EXIT;
				}
			}
		}
		if(backBtn.GetState() == STATE_CLICKED)
			menu = MENU_BROWSE;
	}
	HaltGui();
	mainWindow->Remove(&titleTxt);
	mainWindow->Remove(&browseSmallImg);
	mainWindow->Remove(&buttonWindow);
	mainWindow->Remove(&fileBrowser);
	return menu;
}

/****************************************************************************
 * MenuBrowse
 ***************************************************************************/
static int MenuBrowse()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	int selected = -1;

	GuiImageData sd(sd_png);
	GuiImageData usb(usb_png);
	GuiImageData smb(smb_png);
	GuiImageData dvd(dvd_png);

	MenuItemList items;
	sprintf(items.name[i], "SD Card");
	items.img[i] = &sd; i++;
	sprintf(items.name[i], "USB Mass Storage");
	items.img[i] = &usb; i++;
	sprintf(items.name[i], "Data DVD");
	items.img[i] = &dvd; i++;
	items.name[i][0] = 0;
	items.img[i] = &smb; i++;
	items.name[i][0] = 0;
	items.img[i] = &smb; i++;
	items.name[i][0] = 0;
	items.img[i] = &smb; i++;
	items.name[i][0] = 0;
	items.img[i] = &smb; i++;
	items.name[i][0] = 0;
	items.img[i] = &smb; i++;
	items.length = i;

	for(i=0; i < 5; i++)
	{
		if(smbConf[i].share[0] != 0)
			sprintf(items.name[i+3], "%s (Network)", smbConf[i].share);
	}

	GuiText titleTxt("Browse Files", 36, (GXColor){255, 255, 255, 255});
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

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(selected != itemBrowser.GetSelectedItem())
		{
			selected = itemBrowser.GetSelectedItem();
			bgImg->SetImage(items.img[selected]);
		}

		ret = itemBrowser.GetClickedItem();

		switch (ret)
		{
			case 0:
				currentDevice = DEVICE_SD;
				currentDeviceNum = 0;
				break;

			case 1:
				currentDevice = DEVICE_USB;
				currentDeviceNum = 0;
				break;

			case 2:
				currentDevice = DEVICE_DVD;
				currentDeviceNum = 0;
				break;

			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				currentDevice = DEVICE_SMB;
				currentDeviceNum = ret-3;
				break;
		}

		if(ret >= 0 && ret <= 7)
		{
			if(ChangeInterface(currentDevice, currentDeviceNum, NOTSILENT))
				menu = MENU_BROWSE_DEVICE;
		}

		if(backBtn.GetState() == STATE_CLICKED)
			menu = MENU_MAIN;
	}
	bgImg->SetImage(NULL);
	HaltGui();
	mainWindow->Remove(&itemBrowser);
	mainWindow->Remove(&backBtn);
	mainWindow->Remove(&titleTxt);
	return menu;
}

static int MenuRadio()
{
	return MENU_NONE;
}

static int MenuDVD()
{
	return MENU_NONE;
}

static int MenuOptionsVideo()
{
	int menu = MENU_NONE;
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

	while(menu == MENU_NONE)
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
			menu = MENU_OPTIONS;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}

static int MenuOptionsAudio()
{
	return MENU_NONE;
}

static int MenuOptionsSubtitles()
{
	return MENU_NONE;
}

static int MenuOptionsMenu()
{
	return MENU_NONE;
}

/****************************************************************************
 * MenuOptions
 ***************************************************************************/
static int MenuOptions()
{
	int menu = MENU_NONE;
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

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(selected != itemBrowser.GetSelectedItem())
		{
			selected = itemBrowser.GetSelectedItem();
			bgImg->SetImage(items.img[selected]);
		}

		ret = itemBrowser.GetClickedItem();

		switch (ret)
		{
			case 0:
				menu = MENU_OPTIONS_VIDEO;
				break;

			case 1:
				menu = MENU_OPTIONS_AUDIO;
				break;

			case 2:
				menu = MENU_OPTIONS_SUBTITLES;
				break;

			case 3:
				menu = MENU_OPTIONS_MENU;
				break;
		}

		if(backBtn.GetState() == STATE_CLICKED)
			menu = MENU_MAIN;
	}
	bgImg->SetImage(NULL);
	HaltGui();
	mainWindow->Remove(&itemBrowser);
	mainWindow->Remove(&backBtn);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * MenuMain
 ***************************************************************************/
static int MenuMain()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	int selected = -1;

	GuiImageData browse(browse_png);
	GuiImageData dvd(dvd_png);
	GuiImageData radio(radio_png);
	GuiImageData options(options_png);

	MenuItemList items;
	sprintf(items.name[i], "Browse Files");
	items.img[i] = &browse; i++;
	sprintf(items.name[i], "Play DVD");
	items.img[i] = &dvd; i++;
	sprintf(items.name[i], "Play Radio");
	items.img[i] = &radio; i++;
	sprintf(items.name[i], "Options");
	items.img[i] = &options; i++;
	sprintf(items.name[i], "Exit");
	items.img[i] = NULL; i++;
	items.length = i;

	GuiText titleTxt("Main Menu", 36, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(340,50);

	GuiMenuBrowser itemBrowser(300, 400, &items);
	itemBrowser.SetPosition(280, 120);
	itemBrowser.SetAlignment(ALIGN_LEFT, ALIGN_TOP);

	HaltGui();
	mainWindow->Append(&itemBrowser);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(selected != itemBrowser.GetSelectedItem())
		{
			selected = itemBrowser.GetSelectedItem();
			bgImg->SetImage(items.img[selected]);
		}

		ret = itemBrowser.GetClickedItem();

		switch (ret)
		{
			case 0: // Browse Files
				menu = MENU_BROWSE;
				break;

			case 1: // Play DVD
				menu = MENU_DVD;
				break;

			case 2: // Play Radio
				menu = MENU_RADIO;
				break;

			case 3: // Options
				menu = MENU_OPTIONS;
				break;

			case 4: // Exit
				ExitRequested = 1;
				break;
		}
	}
	bgImg->SetImage(NULL);
	HaltGui();
	mainWindow->Remove(&itemBrowser);
	mainWindow->Remove(&titleTxt);
	return menu;
}

/****************************************************************************
 * Menu
 ***************************************************************************/
void Menu(int menu)
{
	int currentMenu = menu;
	lastMenu = MENU_NONE;

	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);

	mainWindow = new GuiWindow(screenwidth, screenheight);

	bgImg = new GuiImage();
	bgImg->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	bgImg->SetPosition(0, 0);
	GuiImageData bgTop(bg_top_png);
	bgImgTop = new GuiImage(&bgTop);
	bgImgTop->SetVisible(false);
	GuiImageData bgBottom(bg_bottom_png);
	bgImgBottom = new GuiImage(&bgBottom);
	bgImgBottom->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	bgImgBottom->SetVisible(false);

	if(videoScreenshot)
	{
		videoImg = new GuiImage(videoScreenshot, screenwidth, screenheight);
		videoImg->SetAlpha(192);
		videoImg->ColorStripe(30);
		mainWindow->Append(videoImg);
	}

	mainWindow->Append(bgImg);
	mainWindow->Append(bgImgTop);
	mainWindow->Append(bgImgBottom);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	ResumeGui();

	while(currentMenu != MENU_EXIT)
	{
		switch (currentMenu)
		{
			case MENU_BROWSE:
				currentMenu = MenuBrowse();
				break;
			case MENU_BROWSE_DEVICE:
				currentMenu = MenuBrowseDevice();
				break;
			case MENU_DVD:
				currentMenu = MenuDVD();
				break;
			case MENU_RADIO:
				currentMenu = MenuRadio();
				break;
			case MENU_OPTIONS:
				currentMenu = MenuOptions();
				break;
			case MENU_OPTIONS_VIDEO:
				currentMenu = MenuOptionsVideo();
				break;
			case MENU_OPTIONS_AUDIO:
				currentMenu = MenuOptionsAudio();
				break;
			case MENU_OPTIONS_SUBTITLES:
				currentMenu = MenuOptionsSubtitles();
				break;
			case MENU_OPTIONS_MENU:
				currentMenu = MenuOptionsMenu();
				break;
			case MENU_HOME:
				currentMenu = MenuHome();
				break;
			default: // unrecognized menu
				currentMenu = MenuMain();
				break;
		}
		lastMenu = currentMenu;
		usleep(THREAD_SLEEP);
	}

	ShutoffRumble();
	CancelAction();
	HaltGui();

	delete bgImg;
	delete bgImgTop;
	delete bgImgBottom;
	delete mainWindow;

	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];

	mainWindow = NULL;

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
