/*
   MPlayer Wii port

   Copyright (C) 2008 dhewg

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.
*/

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/lwp.h>
#include <debug.h>
#include <wiiuse/wpad.h>

#include <fat.h>
#include <smb.h>
//#include "tinysmbmount.h"
//#include "smb_devoptab.h"

#include <network.h>
#include <di/di.h>
#include "lwiso9660_devoptab.h"
//#include "libiso.h"

#include "log_console.h"
#include "gx_supp.h"
#include "plat_gekko.h"

//#include "../m_config.h"
#include "../m_option.h"
#include "../parser-cfg.h"
#include "../get_path.h"

#undef abort
 
bool ios_reloaded = true;
bool reset_pressed = false;
bool power_pressed = false;
static bool smbmounted = false;

static char *default_args[] = {
	"mplayer.dol",
	"-loop", "0", "sd:/apps/mplayer_ce/loop.avi"
}; 

extern float movie_aspect;

static void reset_cb (void) {
	reset_pressed = true;
}

static void power_cb (void) {
	power_pressed = true;
}

static void wpad_power_cb (void) {
	power_pressed = true;
}

int gekko_gettimeofday(struct timeval *tv, void *tz) {
	u32 us = ticks_to_microsecs(gettime());

	tv->tv_sec = us / TB_USPERSEC;
	tv->tv_usec = us % TB_USPERSEC;

	return 0;
}

void gekko_abort(void) {
	printf("abort() called\n");
	plat_deinit(-1);
	exit(-1);
}

//// Mounting code
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

const DISC_INTERFACE* sd = &__io_wiisd;
const DISC_INTERFACE* usb = &__io_usbstorage;

static bool automountthreadexit = false;
static lwp_t automountthread;

static void * automountthreadfunc (void *arg)
{
  int dvd_inserted = 0;
  int usb_inserted = 0;
  int sd_inserted = 0;
  int dp; 
  
  usb_inserted=usb->isInserted();
  sd_inserted=sd->isInserted();
  
  while(1)
  {
  	if(automountthreadexit) break;
  	
    //DVD code
  	dp = WIIDVD_DiscPresent();
  	if(dvd_inserted != dp) 
    {
      if(dp){
    		WIIDVD_Mount();
    		dvd_inserted = 1;
    	}else{
    		WIIDVD_Unmount();
    		dvd_inserted = 0;
    	}
    }
  	
  	// USB code
  	dp=usb->isInserted();
  	if(usb_inserted != dp)
    { 
    	if(dp){
   		  if(fatMountSimple("usb", usb))
   		    fatSetReadAhead("usb:", 4, 64);
   		  usb_inserted = 1;
    	}else{  		
    		fatUnmount("usb:");
    		usb_inserted = 0;
    	}
  	}
  	
  	// SD code
  	dp=sd->isInserted();
  	if(sd_inserted != dp) 
    {
    	if(dp){
    		if(fatMountSimple("sd", sd))
    		  fatSetReadAhead("sd:", 4, 128);
        sd_inserted = 1;
    	}else{  		
    		fatUnmount("sd:");
    		sd_inserted = 0;
    	}
    }
  	usleep(50000);
  }
  return NULL;
}

bool trysmb(void){
	
	char* smb_ip=NULL;
	char* smb_share=NULL;
	char* smb_user=NULL;
	char* smb_pass=NULL;
	
	m_config_t *smb_conf;
	m_option_t smb_opts[] =
	{
	    {   "ip", &smb_ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share", &smb_share, CONF_TYPE_STRING, 0, 0, 0, NULL },    
	    {   "user", &smb_user, CONF_TYPE_STRING, 0, 0, 0, NULL }, 
	    {   "pass", &smb_pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};
	

	/* read configuration */
	smb_conf = m_config_new();
	m_config_register_options(smb_conf, smb_opts);
	m_config_parse_config_file(smb_conf, "sd:/apps/mplayer_ce/smb.conf");
	
	if(smb_ip==NULL || smb_share==NULL) return false;

	if(smb_user==NULL) smb_user=strdup("");
	if(smb_pass==NULL) smb_pass=strdup("");
		
	return smbInit(smb_user,smb_pass,smb_share,smb_ip);  
}

void plat_init (int *argc, char **argv[]) {
	SetDVDMotorStopSecs(60);  // 1 minute to auto stop dvd motor if no use
	WIIDVD_Init();
	VIDEO_Init();
	PAD_Init();

	AUDIO_Init(NULL);
	AUDIO_RegisterDMACallback(NULL);
	AUDIO_StopDMA();

	SYS_SetResetCallback (reset_cb);
	SYS_SetPowerCallback (power_cb);
	WPAD_SetPowerButtonCallback(wpad_power_cb);

#if 0
	DEBUG_Init(0, 1);
	_break();
#endif

	GX_InitVideo();

	log_console_init(vmode, 128);
  printf("MPlayerCE\n");
  printf("Unofficial MPlayer v.0.1\n\n");
  printf("MPlayer/Wii port (c) 2008 Team Twiizers\n");
  printf("Used Code (c) MPlayerWii[rOn], GeeXboX,\n");
  printf("Play Media files from SD, USB, DATA-DVD, SMB & Radio Streams.\n");
  printf("For SMB Samba Browsing, place smb.conf in apps\mplayer_ce on SD Card!\n");
  printf("Unofficial Modified MPlayer by MPlayerCE Team!\n");
  printf("Scip, Rodries, AgentX, DJDynamite123, Tipolosko, Tantric, etc.\n\n");	
	
	
	//printf("Running under IOS%d\n", IOS_GetVersion());
	//printf("VIDEO: %u * %u (%u * %u)\n", vmode->fbWidth, vmode->efbHeight, vmode->viWidth, vmode->viHeight);

	/*if (!ios_reloaded) {
		printf("IOS could not be reloaded, exiting.\n");
		sleep(3);

		exit(0);
	}*/

	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);
	WPAD_SetIdleTimeout(60);

	if (!fatInitDefault()) {
		printf("fatInit failed\n");
		sleep(3);
		exit(0);
	}

	LWP_CreateThread(&automountthread, automountthreadfunc, NULL, NULL, 0, 80); // auto-mount file system
	
	
  chdir("sd:/apps/mplayer_ce");
	setenv("HOME", "sd:/apps/mplayer_ce", 1);
	setenv("DVDCSS_CACHE", "off", 1);
	//setenv("DVDCSS_VERBOSE", "2", 1);
	setenv("DVDCSS_VERBOSE", "0", 1);
	setenv("DVDREAD_VERBOSE", "0", 1);

 	printf("Waiting for network to initialise..\n");
  char myIP[16];
  if (if_config(myIP, NULL, NULL, true) < 0){
      printf("FAILED\n");
  		//plat_deinit(1); exit(0);
 	}else{  	
      printf("OK [%s]\n", myIP);
      FILE *fp;
	    if ((fp = fopen("sd:/apps/mplayer_ce/smb.conf", "r")) != NULL)
      { 
        printf("Mounting SMB Share.. \n");
  	    smbmounted = trysmb();
  	    if(smbmounted) printf("SMB Share OK\n");
  	    else printf("FAILED\n");
  	  }
  	}

	
  *argv = default_args;
  *argc = sizeof(default_args) / sizeof(char *);
}

void plat_deinit (int rc) {
	
	automountthreadexit = true;
	LWP_JoinThread(automountthread,NULL);

	//WIIDVD_Close();
  //fatUnmount("sd");
  //fatUnmount("usb");

	if (power_pressed) {
		printf("shutting down\n");
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}

	/*
	// only needed to debug problems
  log_console_enable_video(true);

	VIDEO_WaitVSync();

	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	if (rc != 0) sleep(5);
	
	log_console_deinit();
	*/	
	
}


/*
void strcpy_strip_ext2(char *d, char *s)
{
    char *tmp = strrchr(s,'.');
    if (!tmp) {
	strcpy(d, s);
	return;
    } else {
	strncpy(d, s, tmp-s);
	d[tmp-s] = 0;
    }
    while (*d) {
	*d = tolower(*d);
	d++;
    }
}
*/
