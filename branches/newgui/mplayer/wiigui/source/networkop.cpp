/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * networkop.cpp
 * Network and SMB support routines
 ****************************************************************************/

#include <network.h>
#include <smb.h>
#include <unistd.h>

#include "mplayerce.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu.h"
#include "networkop.h"

extern struct SMBSettings smbConf[5];

static bool inNetworkInit = false;
static bool networkInit = false;
static bool autoNetworkInit = true;
static bool networkShareInit = false;

/****************************************************************************
 * InitializeNetwork
 * Initializes the Wii/GameCube network interface
 ***************************************************************************/

void InitializeNetwork(bool silent)
{
	// stop if we're already initialized, or if auto-init has failed before
	// in which case, manual initialization is required
	if(networkInit || !autoNetworkInit)
		return;

	if(!silent)
		ShowAction ("Initializing network...");

	while(inNetworkInit) // a network init is already in progress!
		usleep(50);

	if(networkInit) // check again if the network was inited
		return;

	inNetworkInit = true;

	char ip[16];
	s32 initResult = if_config(ip, NULL, NULL, true);

	if(initResult == 0)
	{
		networkInit = true;
	}
	else
	{
		// do not automatically attempt a reconnection
		autoNetworkInit = false;

		if(!silent)
		{
			char msg[150];
			sprintf(msg, "Unable to initialize network (Error #: %i)", initResult);
			ErrorPrompt(msg);
		}
	}
	if(!silent)
		CancelAction();
	inNetworkInit = false;
}

void CloseShare(int num)
{
	char devName[10];
	sprintf(devName, "smb%d", num+1);

	if(networkShareInit)
		smbClose(devName);
	networkShareInit = false;
	networkInit = false; // trigger a network reinit
}

/****************************************************************************
 * Mount SMB Share
 ****************************************************************************/

bool
ConnectShare (int num, bool silent)
{
	sprintf(rootdir, "smb%d:", num+1);
	char mountpoint[6];
	sprintf(mountpoint, "smb%d", num+1);

	int chkU = (strlen(smbConf[num].user) > 0) ? 0:1;
	int chkP = (strlen(smbConf[num].pwd) > 0) ? 0:1;
	int chkS = (strlen(smbConf[num].share) > 0) ? 0:1;
	int chkI = (strlen(smbConf[num].ip) > 0) ? 0:1;

	// check that all parameters have been set
	if(chkU + chkP + chkS + chkI > 0)
	{
		if(!silent)
		{
			char msg[50];
			char msg2[100];
			if(chkU + chkP + chkS + chkI > 1) // more than one thing is wrong
				sprintf(msg, "Check settings file.");
			else if(chkU)
				sprintf(msg, "Username is blank.");
			else if(chkP)
				sprintf(msg, "Password is blank.");
			else if(chkS)
				sprintf(msg, "Share name is blank.");
			else if(chkI)
				sprintf(msg, "Share IP is blank.");

			sprintf(msg2, "Invalid network settings - %s", msg);
			ErrorPrompt(msg2);
		}
		return false;
	}

	if(unmountRequired[DEVICE_SMB])
		CloseShare(num);

	if(!networkInit)
		InitializeNetwork(silent);

	if(networkInit)
	{
		if(!networkShareInit)
		{
			if(!silent)
				ShowAction ("Connecting to network share...");

			if(smbInitDevice(mountpoint, smbConf[num].user, smbConf[num].pwd,
					smbConf[num].share, smbConf[num].ip))
			{
				networkShareInit = true;
			}
			if(!silent)
				CancelAction();
		}

		if(!networkShareInit && !silent)
			ErrorPrompt("Failed to connect to network share.");
	}

	return networkShareInit;
}
