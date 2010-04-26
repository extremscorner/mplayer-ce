#ifndef _SMBDEVOPTAB_H
#define _SMBDEVOPTAB_H

#ifdef __cplusplus
extern "C" {
#endif

#define SMB_MAXPATH 4096

typedef struct{
	char user[20];
	char pass[20];
	char share[20];
	char ip[16];	
	u16 port;
} SMBCONFIG;


bool smbInit(SMBCONFIG *config, bool setAsDefaultDevice);
bool smbInitDefault(SMBCONFIG *config);
void smbUnmount();

#ifdef __cplusplus
}
#endif

#endif // _SMBDEVOPTAB_H


