/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * filebrowser.cpp
 *
 * Generic file routines - reading, writing, browsing
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <sys/dir.h>
#include <malloc.h>

#include "filebrowser.h"
#include "mplayerce.h"
#include "menu.h"
#include "fileop.h"
#include "networkop.h"

BROWSERINFO browser;
BROWSERENTRY * browserList = NULL; // list of files/folders in browser

/****************************************************************************
 * ResetBrowser()
 * Clears the file browser memory, and allocates one initial entry
 ***************************************************************************/
void ResetBrowser()
{
	browser.numEntries = 0;
	browser.selIndex = 0;
	browser.pageIndex = 0;

	// Clear any existing values
	if(browserList != NULL)
	{
		free(browserList);
		browserList = NULL;
	}
	// set aside space for 1 entry
	browserList = (BROWSERENTRY *)malloc(sizeof(BROWSERENTRY));
	memset(browserList, 0, sizeof(BROWSERENTRY));
	browser.size = 1;
}

bool AddBrowserEntry()
{
	BROWSERENTRY * newBrowserList = (BROWSERENTRY *)realloc(browserList, (browser.size+1) * sizeof(BROWSERENTRY));

	if(!newBrowserList) // failed to allocate required memory
	{
		ResetBrowser();
		ErrorPrompt("Out of memory: too many files!");
		return false;
	}
	else
	{
		browserList = newBrowserList;
	}
	memset(&(browserList[browser.size]), 0, sizeof(BROWSERENTRY)); // clear the new entry
	browser.size++;
	return true;
}

/****************************************************************************
 * CleanupPath()
 * Cleans up the filepath, removing double // and replacing \ with /
 ***************************************************************************/
static void CleanupPath(char * path)
{
	int pathlen = strlen(path);
	int j = 0;
	for(int i=0; i < pathlen && i < MAXPATHLEN; i++)
	{
		if(path[i] == '\\')
			path[i] = '/';

		if(j == 0 || !(path[j-1] == '/' && path[i] == '/'))
			path[j++] = path[i];
	}
	path[j] = 0;
}

bool IsDeviceRoot(char * path)
{
	if(path == NULL || path[0] == 0)
		return false;

	if(strcmp(path, "sd:/") == 0 ||
		strcmp(path, "usb:/") == 0 ||
		strcmp(path, "dvd:/") == 0 ||
		(strncmp(path, "smb", 3) == 0 && strlen(path) == 6))
	{
		return true;
	}
	return false;
}

/****************************************************************************
 * UpdateDirName()
 * Update curent directory name for file browser
 ***************************************************************************/
int UpdateDirName()
{
	int size=0;
	char * test;
	char temp[1024];

	if(browser.numEntries == 0)
		return 1;

	/* current directory doesn't change */
	if (strcmp(browserList[browser.selIndex].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(browserList[browser.selIndex].filename,"..") == 0)
	{
		// already at the top level
		if(IsDeviceRoot(browser.dir))
		{
			browser.dir[0] = 0; // remove device - we are going to the device listing screen
		}
		else
		{
			/* determine last subdirectory namelength */
			sprintf(temp,"%s",browser.dir);
			test = strtok(temp,"/");
			while (test != NULL)
			{
				size = strlen(test);
				test = strtok(NULL,"/");
			}

			/* remove last subdirectory name */
			size = strlen(browser.dir) - size - 1;
			browser.dir[size] = 0;
		}
		return 1;
	}

	if(browser.dir[0] == 0)
	{
		// try to switch to device
		if(!ChangeInterface(browserList[browser.selIndex].filename, NOTSILENT))
			return -1;
	}

	/* Open directory */

	/* test new directory namelength */
	if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) < MAXPATHLEN)
	{
		/* update current directory name */
		sprintf(browser.dir, "%s%s/",browser.dir, browserList[browser.selIndex].filename);
		return 1;
	}
	else
	{
		ErrorPrompt("Directory name is too long!");
		return -1;
	}
}

/****************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((BROWSERENTRY *)f1)->filename[0] == '.' || ((BROWSERENTRY *)f2)->filename[0] == '.')
	{
		if(strcmp(((BROWSERENTRY *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((BROWSERENTRY *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((BROWSERENTRY *)f1)->isdir && !(((BROWSERENTRY *)f2)->isdir)) return -1;
	if(!(((BROWSERENTRY *)f1)->isdir) && ((BROWSERENTRY *)f2)->isdir) return 1;

	return stricmp(((BROWSERENTRY *)f1)->filename, ((BROWSERENTRY *)f2)->filename);
}

/****************************************************************************
 * BrowserChangeFolder
 *
 * Update current directory and set new entry list if directory has changed
 ***************************************************************************/
int BrowserChangeFolder(bool updateDir)
{
	if(updateDir && !UpdateDirName())
		return -1;

	CleanupPath(browser.dir);

	if(browser.dir[0] != 0)
	{
		ParseDirectory();
	}
	else
	{
		// halt parsing
		HaltParseThread();

		// reset browser
		ResetBrowser();

		AddBrowserEntry();
		sprintf(browserList[0].filename, "sd:/");
		sprintf(browserList[0].displayname, "SD Card");
		browserList[0].length = 0;
		browserList[0].mtime = 0;
		browserList[0].isdir = 1;
		browserList[0].icon = ICON_SD;

		AddBrowserEntry();
		sprintf(browserList[1].filename, "usb:/");
		sprintf(browserList[1].displayname, "USB Mass Storage");
		browserList[1].length = 0;
		browserList[1].mtime = 0;
		browserList[1].isdir = 1;
		browserList[1].icon = ICON_USB;

		AddBrowserEntry();
		sprintf(browserList[2].filename, "dvd:/");
		sprintf(browserList[2].displayname, "Data DVD");
		browserList[2].length = 0;
		browserList[2].mtime = 0;
		browserList[2].isdir = 1;
		browserList[2].icon = ICON_DVD;

		browser.numEntries = 3;

		for(int i=0; i < 5; i++)
		{
			if(smbConf[i].share[0] != 0)
			{
				if(!AddBrowserEntry())
					break;

				sprintf(browserList[browser.numEntries].filename, "smb%d:/", i+1);
				sprintf(browserList[browser.numEntries].displayname, "%s (Network)", smbConf[i].share);
				browserList[browser.numEntries].length = 0;
				browserList[browser.numEntries].mtime = 0;
				browserList[browser.numEntries].isdir = 1; // flag this as a dir
				browserList[browser.numEntries].icon = ICON_SMB;
				browser.numEntries++;
			}
		}
	}
	return browser.numEntries;
}
