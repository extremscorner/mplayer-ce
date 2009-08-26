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
int ParseDirectory(bool waitParse = false);

extern bool unmountRequired[];
extern bool isMounted[];
extern lwp_t devicethread;
extern int currentDevice;
extern int currentDeviceNum;

#endif
