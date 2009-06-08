/****************************************************************************
 * MPlayer CE
 * Tantric 2009
 *
 * networkop.h
 * Network and SMB support routines
 ****************************************************************************/

#ifndef _NETWORKOP_H_
#define _NETWORKOP_H_

struct SMBSettings {
	char	ip[16];
	char	share[20];
	char	user[20];
	char	pwd[20];
};

void InitializeNetwork(bool silent);
bool ConnectShare (int num, bool silent);
void CloseShare(int num);

extern struct SMBSettings smbConf[5];

#endif
