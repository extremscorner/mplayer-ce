/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * fileop.cpp
 *
 * File operations
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <malloc.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>

#include "mplayerce.h"
#include "fileop.h"
#include "networkop.h"
#include "menu.h"
#include "filebrowser.h"

int currentDevice = -1;
int currentDeviceNum = -1;
bool unmountRequired[2] = { false, false };
bool isMounted[2] = { false, false };

#ifdef HW_RVL
	const DISC_INTERFACE* sd = &__io_wiisd;
	const DISC_INTERFACE* usb = &__io_usbstorage;
#else
	const DISC_INTERFACE* carda = &__io_gcsda;
	const DISC_INTERFACE* cardb = &__io_gcsdb;
#endif

/****************************************************************************
 * deviceThreading
 ***************************************************************************/
lwp_t devicethread = LWP_THREAD_NULL;
static bool deviceHalt = true;

/****************************************************************************
 * ResumeDeviceThread
 *
 * Signals the device thread to start, and resumes the thread.
 ***************************************************************************/
void
ResumeDeviceThread()
{
	deviceHalt = false;
	LWP_ResumeThread(devicethread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the device thread to stop.
 ***************************************************************************/
void
HaltDeviceThread()
{
	deviceHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(devicethread))
		usleep(100);
}

/****************************************************************************
 * devicecallback
 *
 * This checks our devices for changes (SD/USB removed) and
 * initializes the network in the background
 ***************************************************************************/
static int devsleep = 1*1000*1000;

static void *
devicecallback (void *arg)
{
	while(devsleep > 0)
	{
		if(deviceHalt)
			LWP_SuspendThread(devicethread);
		usleep(100);
		devsleep -= 100;
	}

	while (1)
	{
		if(isMounted[DEVICE_SD])
		{
			if(!sd->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_SD] = true;
				isMounted[DEVICE_SD] = false;
			}
		}

		if(isMounted[DEVICE_USB])
		{
			if(!usb->isInserted()) // check if the device was removed
			{
				unmountRequired[DEVICE_USB] = true;
				isMounted[DEVICE_USB] = false;
			}
		}
		InitializeNetwork(SILENT);
		devsleep = 1000*1000; // 1 sec

		while(devsleep > 0)
		{
			if(deviceHalt)
				LWP_SuspendThread(devicethread);
			usleep(100);
			devsleep -= 100;
		}
	}
	return NULL;
}

/****************************************************************************
 * InitDeviceThread
 *
 * libOGC provides a nice wrapper for LWP access.
 * This function sets up a new local queue and attaches the thread to it.
 ***************************************************************************/
void
InitDeviceThread()
{
	LWP_CreateThread (&devicethread, devicecallback, NULL, NULL, 0, 40);
}

/****************************************************************************
 * UnmountAllFAT
 * Unmounts all FAT devices
 ***************************************************************************/
void UnmountAllFAT()
{
	fatUnmount("sd:/");
	fatUnmount("usb:/");
}

/****************************************************************************
 * MountFAT
 * Checks if the device needs to be (re)mounted
 * If so, unmounts the device
 * Attempts to mount the device specified
 * Sets libfat to use the device by default
 ***************************************************************************/

bool MountFAT(int device)
{
	bool mounted = true; // assume our disc is already mounted
	char name[10];
	const DISC_INTERFACE* disc = NULL;

	switch(device)
	{
		case DEVICE_SD:
			sprintf(name, "sd");
			disc = sd;
			break;
		case DEVICE_USB:
			sprintf(name, "usb");
			disc = usb;
			break;
		default:
			return false; // unknown device
	}

	sprintf(rootdir, "%s:", name);

	if(unmountRequired[device])
	{
		unmountRequired[device] = false;
		fatUnmount(rootdir);
		disc->shutdown();
		isMounted[device] = false;
	}
	if(!isMounted[device])
	{
		if(!disc->startup())
			mounted = false;
		else if(!fatMountSimple(name, disc))
			mounted = false;
	}

	isMounted[device] = mounted;
	return mounted;
}

void MountAllFAT()
{
	MountFAT(DEVICE_SD);
	MountFAT(DEVICE_USB);

}

/****************************************************************************
 * ChangeInterface
 * Attempts to mount/configure the device specified
 ***************************************************************************/
bool ChangeInterface(int device, int devnum, bool silent)
{
	bool mounted = false;

	if(device == DEVICE_SD)
	{
		mounted = MountFAT(DEVICE_SD);
		if(!mounted && !silent)
			ErrorPrompt("SD card not found!");
	}
	else if(device == DEVICE_USB)
	{
		mounted = MountFAT(device);
		if(!mounted && !silent)
			ErrorPrompt("USB drive not found!");
	}
	else if(device == DEVICE_SMB)
	{
		mounted = ConnectShare(devnum, silent);
	}
	return mounted;
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int
ParseDirectory()
{
	DIR_ITER *dir = NULL;
	char fulldir[MAXPATHLEN];
	char filename[MAXPATHLEN];
	struct stat filestat;
	char msg[128];
	int retry = 1;
	bool mounted = false;

	// reset browser
	ResetBrowser();

	ShowAction("Loading...");

	// open the directory
	while(dir == NULL && retry == 1)
	{
		mounted = ChangeInterface(currentDevice, currentDeviceNum, NOTSILENT);
		sprintf(fulldir, "%s%s", rootdir, browser.dir); // add currentDevice to path
		if(mounted) dir = diropen(fulldir);
		if(dir == NULL)
		{
			unmountRequired[currentDevice] = true;
			sprintf(msg, "Error opening %s", fulldir);
			retry = ErrorPromptRetry(msg);
		}
	}

	// if we can't open the dir, try opening the root dir
	if (dir == NULL)
	{
		if(ChangeInterface(currentDevice, currentDeviceNum, SILENT))
		{
			sprintf(browser.dir,"/");
			dir = diropen(rootdir);
			if (dir == NULL)
			{
				sprintf(msg, "Error opening %s", rootdir);
				ErrorPrompt(msg);
				return -1;
			}
		}
	}

	// index files/folders
	int entryNum = 0;

	while(dirnext(dir,filename,&filestat) == 0)
	{
		if(strcmp(filename,".") != 0)
		{
			BROWSERENTRY * newBrowserList = (BROWSERENTRY *)realloc(browserList, (entryNum+1) * sizeof(BROWSERENTRY));

			if(!newBrowserList) // failed to allocate required memory
			{
				ResetBrowser();
				ErrorPrompt("Out of memory: too many files!");
				entryNum = -1;
				break;
			}
			else
			{
				browserList = newBrowserList;
			}
			memset(&(browserList[entryNum]), 0, sizeof(BROWSERENTRY)); // clear the new entry

			strncpy(browserList[entryNum].filename, filename, MAXJOLIET);

			if(strcmp(filename,"..") == 0)
			{
				sprintf(browserList[entryNum].displayname, "Up One Level");
			}
			else
			{
				strncpy(browserList[entryNum].displayname, filename, MAXDISPLAY);	// crop name for display
			}

			browserList[entryNum].length = filestat.st_size;
			browserList[entryNum].isdir = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1; // flag this as a dir

			entryNum++;
		}
	}

	// close directory
	dirclose(dir);

	// Sort the file list
	qsort(browserList, entryNum, sizeof(BROWSERENTRY), FileSortCallback);

	CancelAction();

	browser.numEntries = entryNum;
	return entryNum;
}
