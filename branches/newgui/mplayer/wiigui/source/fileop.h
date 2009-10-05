/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * fileop.h
 *
 * File operations
 ****************************************************************************/

#ifndef _FILEOP_H_
#define _FILEOP_H_

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <fat.h>
#include <unistd.h>

void ResumeDeviceThread();
void HaltDeviceThread();
void HaltParseThread();
void MountAllFAT();
void UnmountAllFAT();
bool ChangeInterface(int device, int devnum, bool silent);
bool ChangeInterface(char * filepath, bool silent);
void CreateAppPath(char * origpath);
int ParseDirectory(bool waitParse = false);
int LoadPlaylist();
int ParsePlaylist();
int ParseOnlineMedia();
size_t LoadFile (char * buffer, char *filepath, bool silent);
size_t SaveFile (char * buffer, char *filepath, size_t datasize, bool silent);

extern bool unmountRequired[];
extern bool isMounted[];
extern lwp_t devicethread;
extern int currentDevice;
extern int currentDeviceNum;
extern int selectLoadedFile;

#endif
