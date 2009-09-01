/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * fileop.cpp
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

#define THREAD_SLEEP 100

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

// folder parsing thread
static lwp_t parsethread = LWP_THREAD_NULL;
static DIR_ITER * dirIter = NULL;
static bool parseHalt = true;
bool ParseDirEntries();

// device thread
lwp_t devicethread = LWP_THREAD_NULL;
static bool deviceHalt = true;

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
			return NULL;
		usleep(THREAD_SLEEP);
		devsleep -= THREAD_SLEEP;
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
				return NULL;
			usleep(THREAD_SLEEP);
			devsleep -= THREAD_SLEEP;
		}
	}
	return NULL;
}

static void *
parsecallback (void *arg)
{
	while(ParseDirEntries()) usleep(THREAD_SLEEP);
	return NULL;
}

/****************************************************************************
 * ResumeDeviceThread
 *
 * Signals the device thread to start, and resumes the thread.
 ***************************************************************************/
void
ResumeDeviceThread()
{
	deviceHalt = false;
	if(devicethread == LWP_THREAD_NULL)
		LWP_CreateThread (&devicethread, devicecallback, NULL, NULL, 0, 40);
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

	if(devicethread != LWP_THREAD_NULL)
	{
		// wait for thread to finish
		LWP_JoinThread(devicethread, NULL);
		devicethread = LWP_THREAD_NULL;
	}
}

/****************************************************************************
 * ResumeParseThread
 *
 * Signals the parse thread to start, and resumes the thread.
 ***************************************************************************/
void
ResumeParseThread()
{
	parseHalt = false;
	if(parsethread == LWP_THREAD_NULL)
		LWP_CreateThread (&parsethread, parsecallback, NULL, NULL, 0, 80);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the parse thread to stop.
 ***************************************************************************/
void
HaltParseThread()
{
	parseHalt = true;

	if(parsethread != LWP_THREAD_NULL)
	{
		// wait for thread to finish
		LWP_JoinThread(parsethread, NULL);
		parsethread = LWP_THREAD_NULL;
	}
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
	char name[10], name2[10];
	const DISC_INTERFACE* disc = NULL;

	switch(device)
	{
		case DEVICE_SD:
			sprintf(name, "sd");
			sprintf(name2, "sd:");
			disc = sd;
			break;
		case DEVICE_USB:
			sprintf(name, "usb");
			sprintf(name2, "usb:");
			disc = usb;
			break;
		default:
			return false; // unknown device
	}

	if(unmountRequired[device])
	{
		unmountRequired[device] = false;
		fatUnmount(name2);
		disc->shutdown();
		isMounted[device] = false;
	}
	if(!isMounted[device])
	{
		if(!disc->startup())
			mounted = false;
		else if(!fatMount(name, disc, 0, 2, 256))
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

static bool FindDevice(char * filepath, int * device, int * devnum)
{
	int tmp = -1;

	if(strncmp(filepath, "sd:", 3) == 0)
	{
		*device = DEVICE_SD;
		return true;
	}
	else if(strncmp(filepath, "usb:", 4) == 0)
	{
		*device = DEVICE_USB;
		return true;
	}
	else if(strncmp(filepath, "dvd:", 4) == 0)
	{
		*device = DEVICE_DVD;
		return true;
	}
	else if(strncmp(filepath, "smb", 3) == 0)
	{
		tmp = atoi(&filepath[3]);
		if(tmp > 0 && tmp < 6)
		{
			*device = DEVICE_SMB;
			*devnum = tmp;
			return true;
		}
	}
	else if(strncmp(filepath, "ftp", 3) == 0)
	{
		tmp = atoi(&filepath[3]);
		if(tmp > 0 && tmp < 6)
		{
			*device = DEVICE_FTP;
			*devnum = tmp;
			return true;
		}
	}
	return false;
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
	else if(device == DEVICE_DVD)
	{
		mounted = DVDGekkoMount();
		if(!mounted && !silent)
			ErrorPrompt("DVD not found!");
	}
	else if(device == DEVICE_SMB)
	{
		mounted = ConnectShare(devnum, silent);
	}

	if(mounted)
	{
		currentDevice = device;
		currentDeviceNum = devnum;
	}

	return mounted;
}

bool ChangeInterface(char * filepath, bool silent)
{
	int device = -1;
	int devnum = -1;

	if(!FindDevice(filepath, &device, &devnum))
		return false;

	return ChangeInterface(device, devnum, silent);
}

void CreateAppPath(char * origpath)
{
	appPath[0] = 0;

	char * path = strdup(origpath); // make a copy so we don't mess up original

	// replace fat:/ with sd:/
	if(strncmp(path, "fat:/", 5) == 0)
	{
		path = path + 1;
		path[0] = 's';
		path[1] = 'd';
	}
	if(ChangeInterface(path, SILENT))
		strncpy(appPath, path, 1024);
	free(path);
}

bool ParseDirEntries()
{
	if(!dirIter)
		return false;

	char filename[MAXPATHLEN];
	struct stat filestat;

	int i, res;

	for(i=0; i < 20; i++)
	{
		res = dirnext(dirIter,filename,&filestat);

		if(res != 0)
			break;

		if(strcmp(filename,".") == 0)
		{
			i--;
			continue;
		}

		if(AddBrowserEntry())
		{
			strncpy(browserList[browser.numEntries+i].filename, filename, MAXJOLIET);
			browserList[browser.numEntries+i].length = filestat.st_size;
			browserList[browser.numEntries+i].mtime = filestat.st_mtime;
			browserList[browser.numEntries+i].isdir = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1; // flag this as a dir

			if(browserList[browser.numEntries+i].isdir)
			{
				if(strcmp(filename, "..") == 0)
					sprintf(browserList[browser.numEntries+i].displayname, "Up One Level");
				else
					strncpy(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename, MAXJOLIET);
				browserList[browser.numEntries+i].icon = ICON_FOLDER;
			}
			else
			{
				strncpy(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename, MAXJOLIET);
				browserList[browser.numEntries+i].icon = ICON_NONE;
			}
		}
		else
		{
			i = -1;
			parseHalt = true;
		}
	}

	// Sort the file list
	if(i >= 0)
	{
		browser.numEntries += i;
		qsort(browserList, browser.numEntries, sizeof(BROWSERENTRY), FileSortCallback);
	}

	if(res != 0 || parseHalt)
	{
		dirclose(dirIter); // close directory
		dirIter = NULL;
		return false; // no more entries
	}
	return true; // more entries
}

/***************************************************************************
 * Browse subdirectories
 **************************************************************************/
int
ParseDirectory(bool waitParse)
{
	char msg[128];
	int retry = 1;
	bool mounted = false;

	// halt parsing
	HaltParseThread();

	// reset browser
	ResetBrowser();

	// open the directory
	while(dirIter == NULL && retry == 1)
	{
		mounted = ChangeInterface(browser.dir, NOTSILENT);
		if(mounted) dirIter = diropen(browser.dir);

		if(dirIter == NULL)
		{
			sprintf(msg, "Error opening %s", browser.dir);
			retry = ErrorPromptRetry(msg);
		}
	}

	// if we can't open the dir, try opening the root dir
	if (dirIter == NULL && !IsDeviceRoot(browser.dir))
	{
		if(ChangeInterface(browser.dir, SILENT))
		{
			char * devEnd = strchr(browser.dir, '/');
			devEnd[1] = 0; // strip remaining file listing
			dirIter = diropen(browser.dir);
			if (dirIter == NULL)
			{
				sprintf(msg, "Error opening %s", browser.dir);
				ErrorPrompt(msg);
				return -1;
			}
		}
	}

	if(IsDeviceRoot(browser.dir))
	{
		browser.numEntries = 1;
		sprintf(browserList[0].filename, "..");
		sprintf(browserList[0].displayname, "Up One Level");
		browserList[0].length = 0;
		browserList[0].mtime = 0;
		browserList[0].isdir = 1; // flag this as a dir
	}

	parseHalt = false;
	ParseDirEntries(); // index first 20 entries
	ResumeParseThread(); // index remaining entries

	if(waitParse) // wait for complete parsing
	{
		ShowAction("Loading...");

		if(parsethread != LWP_THREAD_NULL)
		{
			LWP_JoinThread(parsethread, NULL);
			parsethread = LWP_THREAD_NULL;
		}

		CancelAction();
	}

	return browser.numEntries;
}

/****************************************************************************
 * LoadFile
 ***************************************************************************/
u32 LoadFile (char * buffer, char *filepath, bool silent)
{
	u32 size = 0;
	u32 readsize = 0;
	int retry = 1;
	FILE * file;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceThread();

	// halt parsing
	parseHalt = true;

	// open the file
	while(!size && retry == 1)
	{
		if(ChangeInterface(filepath, silent))
		{
			file = fopen (filepath, "rb");

			if(file > 0)
			{
				struct stat fileinfo;
				if(fstat(file->_file, &fileinfo) == 0)
				{
					size = fileinfo.st_size;

					u32 offset = 0;
					u32 nextread = 0;
					while(offset < size)
					{
						if(size - offset > 4*1024) nextread = 4*1024;
						else nextread = size-offset;
						ShowProgress ("Loading...", offset, size);
						readsize = fread (buffer + offset, 1, nextread, file); // read in next chunk

						if(readsize <= 0 || readsize > nextread)
							break; // read failure

						if(readsize > 0)
							offset += readsize;
					}
					CancelAction();

					if(offset != size) // # bytes read doesn't match # expected
						size = 0;
				}
				fclose (file);
			}
		}
		if(!size)
		{
			if(!silent)
			{
				retry = ErrorPromptRetry("Error loading file!");
			}
			else
			{
				retry = 0;
			}
		}
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();
	CancelAction();
	return size;
}

/****************************************************************************
 * SaveFile
 * Write buffer to file
 ***************************************************************************/
u32 SaveFile (char * buffer, char *filepath, u32 datasize, bool silent)
{
	u32 written = 0;
	int retry = 1;
	FILE * file;

	if(datasize == 0)
		return 0;

	ShowAction("Saving...");

	// stop checking if devices were removed/inserted
	// since we're saving a file
	HaltDeviceThread();

	while(!written && retry == 1)
	{
		if(ChangeInterface(filepath, silent))
		{
			file = fopen (filepath, "wb");

			if (file > 0)
			{
				u32 writesize, nextwrite;
				while(written < datasize)
				{
					if(datasize - written > 4*1024) nextwrite=4*1024;
					else nextwrite = datasize-written;
					writesize = fwrite (buffer+written, 1, nextwrite, file);
					if(writesize != nextwrite) break; // write failure
					written += writesize;
				}

				if(written != datasize) written = 0;
				fclose (file);
			}
		}
		if(!written)
		{
			if(!silent)
				retry = ErrorPromptRetry("Error saving file!");
			else
				retry = 0;
		}
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();

	CancelAction();
    return written;
}
