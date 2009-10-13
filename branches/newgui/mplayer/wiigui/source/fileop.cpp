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

extern "C" {
#include "../../playtree.h"
}

#include "mplayerce.h"
#include "fileop.h"
#include "networkop.h"
#include "menu.h"
#include "filebrowser.h"
#include "settings.h"
#include "libwiigui/gui.h"

#define THREAD_SLEEP 100

int currentDevice = -1;
int currentDeviceNum = -1;
bool unmountRequired[3] = { false, false, false };
bool isMounted[3] = { false, false, false };

const DISC_INTERFACE* sd = &__io_wiisd;
const DISC_INTERFACE* usb = &__io_usbstorage;

// folder parsing thread
static lwp_t parsethread = LWP_THREAD_NULL;
static DIR_ITER * dirIter = NULL;
static bool parseHalt = true;
bool ParseDirEntries();
int selectLoadedFile = 0;

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

		if(isMounted[DEVICE_DVD])
		{
			if(!WIIDVD_DiscPresent())
			{
				unmountRequired[DEVICE_DVD] = true;
				isMounted[DEVICE_DVD] = false;
			}
		}

		UpdateCheck();
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
		LWP_CreateThread (&parsethread, parsecallback, NULL, NULL, 0, 40);
}

/****************************************************************************
 * HaltParseThread
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

static bool MountFAT(int device, int silent)
{
	bool mounted = false;
	int retry = 1;
	char name[10], name2[10];
	const DISC_INTERFACE* disc = NULL;

	if(isMounted[device])
		return true;

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

	while(retry)
	{
		if(disc->startup() && fatMount(name, disc, 0, 2, 256))
			mounted = true;

		if(mounted || silent)
			break;

		if(device == DEVICE_SD)
			retry = ErrorPromptRetry("SD card not found!");
		else
			retry = ErrorPromptRetry("USB drive not found!");
	}

	isMounted[device] = mounted;
	return mounted;
}

void MountAllFAT()
{
	MountFAT(DEVICE_SD, SILENT);
	MountFAT(DEVICE_USB, SILENT);
}

/****************************************************************************
 * MountDVD()
 *
 * Tests if a ISO9660 DVD is inserted and available, and mounts it
 ***************************************************************************/
bool MountDVD(bool silent)
{
	bool mounted = false;
	int retry = 1;

	if(isMounted[DEVICE_DVD])
		return true;

	if(unmountRequired[DEVICE_DVD])
	{
		unmountRequired[DEVICE_DVD] = false;
		WIIDVD_Unmount();
	}

	while(retry)
	{
		ShowAction("Loading DVD...");

		if(!WIIDVD_DiscPresent())
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("No disc inserted!");
		}
		else if(WIIDVD_Mount() < 0)
		{
			if(silent)
				break;
			
			retry = ErrorPromptRetry("Invalid DVD.");
		}
		else
		{
			mounted = true;
			break;
		}
	}
	CancelAction();
	isMounted[DEVICE_DVD] = mounted;
	return mounted;
}

static bool FindDevice(char * filepath, int * device, int * devnum)
{
	if(!filepath || filepath[0] == 0)
		return false;

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
	
	switch(device)
	{
		case DEVICE_SD:
		case DEVICE_USB:
			mounted = MountFAT(device, silent);
			break;
		case DEVICE_DVD:
			mounted = MountDVD(silent);
			break;
		case DEVICE_SMB:
			mounted = ConnectShare(devnum, silent);
			break;
		case DEVICE_FTP:
			mounted = ConnectFTP(devnum, silent);
			break;
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
	if(!filepath || filepath[0] == 0)
		return false;

	int device = -1;
	int devnum = -1;

	if(!FindDevice(filepath, &device, &devnum))
		return false;

	return ChangeInterface(device, devnum, silent);
}

void CreateAppPath(char * origpath)
{
	if(!origpath || origpath[0] == 0)
		return;

	char * path = strdup(origpath); // make a copy so we don't mess up original

	if(!path)
		return;
	
	char * loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	int pos = 0;

	// replace fat:/ with sd:/
	if(strncmp(path, "fat:/", 5) == 0)
	{
		pos++;
		path[1] = 's';
		path[2] = 'd';
	}
	if(ChangeInterface(&path[pos], SILENT))
		strncpy(appPath, &path[pos], MAXPATHLEN);
	appPath[MAXPATHLEN-1] = 0;
	free(path);
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
 ***************************************************************************/
void StripExt(char* string)
{
	char* loc_dot;

	if(string == NULL || strlen(string) < 4)
		return;

	loc_dot = strrchr(string,'.');
	if (loc_dot != NULL)
		*loc_dot = 0; // strip file extension
}

/****************************************************************************
 * CleanFilename
 *
 * Strips out all of the useless nonsense from a filename
 ***************************************************************************/
void CleanFilename(char* string)
{
	if(string == NULL || strlen(string) < 4)
		return;
}

bool ParseDirEntries()
{
	if(!dirIter)
		return false;

	char filename[MAXPATHLEN];
	char *ext;
	struct stat filestat;
	bool isPlaylist;

	int i = 0;
	int j, res;

	while(i < 20)
	{
		isPlaylist = false; // assume this file is not a playlist
		res = dirnext(dirIter,filename,&filestat);

		if(res != 0)
			break;

		if(strcmp(filename,".") == 0)
			continue;
		
		ext = strrchr(filename,'.');
		
		if(ext != NULL)
		{		
			ext++;
			
			// check if this is a playlist
			j=0;
			do
			{
				if (!strcasecmp(ext, validPlaylistExtensions[j++]))
				{
					isPlaylist = true;
					break;
				}
			} while (validPlaylistExtensions[j][0] != 0);
		}

		// check that this file's extension is on the list of visible file types
		if(CESettings.filterFiles && (filestat.st_mode & _IFDIR) == 0 && !isPlaylist)
		{
			if(ext == NULL)
				continue; // file does not have an extension - skip it

			j=0;
			
			if(currentMenu == MENU_BROWSE_VIDEOS)
			{
				do
				{
					if (strcasecmp(ext, validVideoExtensions[j]) == 0)
						break;
				} while (validVideoExtensions[++j][0] != 0);
				if (validVideoExtensions[j][0] == 0) // extension not found
					continue;
			}
			else if(currentMenu == MENU_BROWSE_MUSIC)
			{
				do
				{
					if (strcasecmp(ext, validAudioExtensions[j]) == 0)
						break;
				} while (validAudioExtensions[++j][0] != 0);
				if (validAudioExtensions[j][0] == 0) // extension not found
					continue;
			}
		}

		// add the entry
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
				if(isPlaylist)
					browserList[browser.numEntries+i].isplaylist = 1;
				
				strncpy(browserList[browser.numEntries+i].displayname, browserList[browser.numEntries+i].filename, MAXJOLIET);
				browserList[browser.numEntries+i].icon = ICON_NONE;

				// hide the file's extension
				if(CESettings.hideExtensions)
					StripExt(browserList[browser.numEntries+i].displayname);

				// strip unwanted stuff from the filename
				if(CESettings.cleanFilenames)
					CleanFilename(browserList[browser.numEntries+i].displayname);
			}
			i++;
		}
		else
		{
			parseHalt = true;
		}
	}

	// Sort the file list
	if(i > 0)
	{
		qsort(browserList, browser.numEntries+i, sizeof(BROWSERENTRY), FileSortCallback);
	}

	// try to find and select the last loaded file
	if(selectLoadedFile == 1 && res != 0 && loadedFile[0] != 0 && browser.dir[0] != 0)
	{
		int indexFound = -1;
		int dirLen = strlen(browser.dir);
		int fileLen = strlen(loadedFile);
		char file[MAXPATHLEN];
		
		if(fileLen > dirLen && strncmp(loadedFile, browser.dir, dirLen) == 0)
			strcpy(file, &loadedFile[dirLen]);
		else
			strcpy(file, loadedFile);
		
		for(j=1; j < browser.numEntries + i; j++)
		{
			if(strcmp(browserList[j].filename, file) == 0)
			{
				indexFound = j;
				break;
			}
		}

		// move to this file
		if(indexFound > 0)
		{
			if(indexFound > FILE_PAGESIZE)
			{			
				browser.pageIndex = (ceil(indexFound/FILE_PAGESIZE*1.0)) * FILE_PAGESIZE;
				
				if(browser.pageIndex + FILE_PAGESIZE > browser.numEntries + i)
				{
					browser.pageIndex = browser.numEntries + i - FILE_PAGESIZE;
				}
			}
			browser.selIndex = indexFound;
		}
		selectLoadedFile = 2; // selecting done
	}
	
	browser.numEntries += i;

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

	ResetBrowser(); // reset browser

	// open the directory
	while(dirIter == NULL && retry == 1)
	{
		mounted = ChangeInterface(browser.dir, NOTSILENT);

		if(mounted)
			dirIter = diropen(browser.dir);
		else
			return -1;

		if(dirIter == NULL)
		{
			sprintf(msg, "Error opening %s", browser.dir);
			retry = ErrorPromptRetry(msg);
		}
	}

	// if we can't open the dir, try higher levels
	if (dirIter == NULL)
	{
		char * devEnd = strrchr(browser.dir, '/');

		while(!IsDeviceRoot(browser.dir))
		{
			devEnd[0] = 0; // strip slash
			devEnd = strrchr(browser.dir, '/');

			if(devEnd == NULL)
				break;

			devEnd[1] = 0; // strip remaining file listing
			dirIter = diropen(browser.dir);
			if (dirIter)
				break;
		}
	}
	
	if(dirIter == NULL)
		return -1;

	if(IsDeviceRoot(browser.dir))
	{
		browser.numEntries = 1;
		sprintf(browserList[0].filename, "..");
		sprintf(browserList[0].displayname, "Up One Level");
		browserList[0].length = 0;
		browserList[0].mtime = 0;
		browserList[0].isdir = 1; // flag this as a dir
		browserList[0].icon = ICON_FOLDER;
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

int LoadPlaylist()
{
	char * playlistEntry;
	
	play_tree_t * list = parse_playlist_file(currentPlaylist);
	
	if(!list)
		return 0;
	
	if(playlist)
	{
		free(playlist);
		playlist = NULL;
	}

	playlistSize = 0;
	
	play_tree_iter_t *pt_iter = NULL;

	if((pt_iter = pt_iter_create(&list, NULL)))
	{
		while ((playlistEntry = pt_iter_get_next_file(pt_iter)) != NULL)
		{
			playlist = (MEDIAENTRY *)realloc(playlist, (playlistSize + 1) * sizeof(MEDIAENTRY));
			memset(&(playlist[playlistSize]), 0, sizeof(MEDIAENTRY)); // clear the new entry
			strncpy(playlist[playlistSize].address, playlistEntry, MAXPATHLEN);
			playlistSize++;
		}
		pt_iter_destroy(&pt_iter);
	}
	return playlistSize;
}

int ParsePlaylist()
{
	AddBrowserEntry();
	sprintf(browserList[0].filename, "..");
	sprintf(browserList[0].displayname, "Up One Level");
	browserList[0].length = 0;
	browserList[0].mtime = 0;
	browserList[0].isdir = 1;
	browserList[0].icon = ICON_FOLDER;
	browser.numEntries++;
	
	int i;
	char * start;
	
	for(i=0; i < playlistSize; i++)
	{
		AddBrowserEntry();
		sprintf(browserList[i+1].filename, playlist[i].address);
		start = strrchr(playlist[i].address,'/');
		if(start != NULL) // start up starting part of path
		{
			start++;
			sprintf(browserList[i+1].displayname, start);
		}
		else
		{
			sprintf(browserList[i+1].displayname, playlist[i].address);
		}
		browserList[i+1].length = 0;
		browserList[i+1].mtime = 0;
		browserList[i+1].isdir = 0;
		browserList[i+1].isplaylist = 0;
		browserList[i+1].icon = ICON_NONE;
	}
	browser.numEntries += i;
	return browser.numEntries;
}

int ParseOnlineMedia()
{
	if(browser.dir[0] != 0)
	{
		AddBrowserEntry();
		sprintf(browserList[0].filename, "..");
		sprintf(browserList[0].displayname, "Up One Level");
		browserList[0].length = 0;
		browserList[0].mtime = 0;
		browserList[0].isdir = 1;
		browserList[0].icon = ICON_FOLDER;
		browser.numEntries++;
	}
	
	for(int i=0; i < onlinemediaSize; i++)
	{
		int filepathLen = strlen(onlinemediaList[i].filepath);
		int dirLen = strlen(browser.dir);
		
		// add file
		if(strcmp(browser.dir, onlinemediaList[i].filepath) == 0)
		{
			AddBrowserEntry();
			strncpy(browserList[browser.numEntries].filename, onlinemediaList[i].address, MAXPATHLEN);
			strncpy(browserList[browser.numEntries].displayname, onlinemediaList[i].displayname, MAXJOLIET);
			browserList[browser.numEntries].length = 0;
			browserList[browser.numEntries].mtime = 0;
			browserList[browser.numEntries].isdir = 0;
			browserList[browser.numEntries].isplaylist = 1;
			browserList[browser.numEntries].icon = ICON_NONE;
			browser.numEntries++;
		}
		else if(filepathLen > dirLen && 
			strncmp(browser.dir, onlinemediaList[i].filepath, dirLen) == 0)
		{
			char folder[MAXPATHLEN];
			strncpy(folder, &onlinemediaList[i].filepath[dirLen], MAXPATHLEN);
			char * end = strchr(folder, '/');
			if(end) *end = 0; // strip end
			
			// check if this folder was already added
			bool matchFound = false;

			for(int j=0; j < browser.numEntries; j++)
			{
				if(strcmp(browserList[j].filename, folder) == 0)
				{
					matchFound = true;
					break;
				}
			}
			
			if(!matchFound)
			{			
				// add the folder
				AddBrowserEntry();
				strncpy(browserList[browser.numEntries].filename, folder, MAXPATHLEN);
				strncpy(browserList[browser.numEntries].displayname, folder, MAXJOLIET);
				browserList[browser.numEntries].length = 0;
				browserList[browser.numEntries].mtime = 0;
				browserList[browser.numEntries].isdir = 1;
				browserList[browser.numEntries].icon = ICON_FOLDER;
				browser.numEntries++;
			}
		}
	}
	
	// Sort the file list
	qsort(browserList, browser.numEntries, sizeof(BROWSERENTRY), FileSortCallback);
	return browser.numEntries;
}

/****************************************************************************
 * LoadFile
 ***************************************************************************/
size_t LoadFile (char * buffer, char *filepath, bool silent)
{
	size_t size = 0, offset = 0, readsize = 0;
	int retry = 1;
	FILE * file;

	// stop checking if devices were removed/inserted
	// since we're loading a file
	HaltDeviceThread();

	// halt parsing
	HaltParseThread();

	// open the file
	while(!size && retry)
	{
		if(!ChangeInterface(filepath, silent))
			break;

		file = fopen (filepath, "rb");

		if(!file)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error opening file!");
			continue;
		}

		fseeko(file,0,SEEK_END);
		size = ftello(file);
		fseeko(file,0,SEEK_SET);

		while(!feof(file))
		{
			ShowProgress ("Loading...", offset, size);
			readsize = fread (buffer + offset, 1, 4096, file); // read in next chunk

			if(readsize <= 0)
				break; // reading finished (or failed)

			offset += readsize;
		}
		fclose (file);
		size = offset;
		CancelAction();
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
size_t SaveFile (char * buffer, char *filepath, size_t datasize, bool silent)
{
	size_t written = 0;
	size_t writesize, nextwrite;
	int retry = 1;
	FILE * file;

	if(datasize == 0)
		return 0;

	ShowAction("Saving...");

	// stop checking if devices were removed/inserted
	// since we're saving a file
	HaltDeviceThread();

	// halt parsing
	HaltParseThread();

	while(!written && retry)
	{
		if(!ChangeInterface(filepath, silent))
			break;

		file = fopen (filepath, "wb");

		if(!file)
		{
			if(silent)
				break;

			retry = ErrorPromptRetry("Error creating file!");
			continue;
		}

		while(written < datasize)
		{
			if(datasize - written > 4096) nextwrite=4096;
			else nextwrite = datasize-written;
			writesize = fwrite (buffer+written, 1, nextwrite, file);
			if(writesize != nextwrite) break; // write failure
			written += writesize;
		}
		fclose (file);

		if(written != datasize) written = 0;

		if(!written)
		{
			retry = ErrorPromptRetry("Error saving file!");
		}
	}

	// go back to checking if devices were inserted/removed
	ResumeDeviceThread();

	CancelAction();
    return written;
}
