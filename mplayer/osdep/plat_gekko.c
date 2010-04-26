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

   Improved by MplayerCE Team
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

#include <network.h>
#include <errno.h>
#include <di/di.h>
#include "lwiso9660_devoptab.h"


#include "log_console.h"
#include "gx_supp.h"
#include "plat_gekko.h"

#include "../m_option.h"
#include "../parser-cfg.h"
#include "../get_path.h"

#undef abort
 

bool reset_pressed = false;
bool power_pressed = false;
bool playing_usb = false;
static bool dvd_mounted = false;
static bool dvd_mounting = false;
static bool dbg_network = false;
static bool exit_automount_thread = false;

static char *default_args[] = {
	"mplayer.dol",
    "-really-quiet","-lavdopts","lowres=1,900:fast=1:skiploopfilter=all","-bgvideo", "sd:/apps/mplayer_ce/loop.avi", "-idle", "sd:/apps/mplayer_ce/loop.avi"
}; 

//extern float movie_aspect;

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
	//printf("abort() called\n");
	plat_deinit(-1);
	exit(-1);
}

void trysmb();

//// Mounting code
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

const DISC_INTERFACE* sd = &__io_wiisd;
const DISC_INTERFACE* usb = &__io_usbstorage;

static lwp_t mainthread;

static s32 initialise_network() 
{
    s32 result;
    while ((result = net_init()) == -EAGAIN) usleep(500);
    return result;
}

int wait_for_network_initialisation() 
{
	static int inited = 0;
	if(inited) return 1;
	
    if (initialise_network() >= 0) {
        char myIP[16];
        if (if_config(myIP, NULL, NULL, true) < 0)
		{
		  if(dbg_network) printf("Error getting ip\n");
		  return 0;
		}
        else
		{
		  inited = 1;
		  if(dbg_network) printf("Netwok initialized. IP: %s\n",myIP);
		  return 1;
		}
    }
	if(dbg_network) printf("Error initializing netwok\n");
	return 0;
}


bool DVDMount()
{
	if(dvd_mounted) return true;
	if(WIIDVD_DiscPresent())
	{
		bool ret;
		dvd_mounting=true;
		WIIDVD_Unmount();
		ret = WIIDVD_Mount();
		dvd_mounted=true;
		dvd_mounting=false;
		if(ret==-1) return false;
		return true;		
	}
	dvd_mounted=false;
	return false;
}

static void * networkthreadfunc (void *arg)
{
	int i;
	for(i=0;i<3;i++) // 3 retrys
	{
		if(wait_for_network_initialisation()) 
		{
			trysmb();
			break;
		}else usleep(3000);
	}
	LWP_JoinThread(mainthread,NULL);
	return NULL;
}

#include <sys/iosupport.h>
bool DeviceMounted(const char *device)
{
  devoptab_t *devops;
  int i;
  devops = (devoptab_t*)GetDeviceOpTab(device);
  if (!devops) return false;
  for(i=0;device[i]!='\0' && device[i]!=':';i++);  
  if (!devops || strncmp(device,devops->name,i)) return false;
  return true;
}

static void * mountthreadfunc (void *arg)
{
	int dp, dvd_inserted=0,usb_inserted=0;

	//initialize usb
	usb->startup();
	if(usb->isInserted())		
	{
		if(fatMountSimple("usb",usb))
		{
		fatSetReadAhead("usb:", 4, 64);
		usb_inserted=1;
		}
	}
	sleep(2);
	while(!exit_automount_thread)
	{	
		if(!playing_usb)
		{
			dp=usb->isInserted();
			usleep(500); // needed, I don't know why, but hang if it's deleted
			
			if(dp!=usb_inserted)
			{
				usb_inserted=dp;
				if(!dp)
				{
					fatUnmount("usb:");
				}
			}
		}				
		if(dvd_mounting==false)
		{
			dp=WIIDVD_DiscPresent();
			if(dp!=dvd_inserted)
			{
				dvd_inserted=dp;
				if(!dp)dvd_mounted=false; // eject
			}
		}
		
		sleep(1);
	}
	LWP_JoinThread(mainthread,NULL);
	return NULL;
}

void mount_smb(int number)
{
	
	char* smb_ip=NULL;
	char* smb_share=NULL;
	char* smb_user=NULL;
	char* smb_pass=NULL;
	
	m_config_t *smb_conf;
	m_option_t smb_opts[] =
	{
	    {   NULL, &smb_ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   NULL, &smb_share, CONF_TYPE_STRING, 0, 0, 0, NULL },    
	    {   NULL, &smb_user, CONF_TYPE_STRING, 0, 0, 0, NULL }, 
	    {   NULL, &smb_pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};
	char cad[10];
	sprintf(cad,"ip%d",number);smb_opts[0].name=strdup(cad);	
	sprintf(cad,"share%d",number);smb_opts[1].name=strdup(cad);
	sprintf(cad,"user%d",number);smb_opts[2].name=strdup(cad);
	sprintf(cad,"pass%d",number);smb_opts[3].name=strdup(cad);

	/* read configuration */
	smb_conf = m_config_new();
	m_config_register_options(smb_conf, smb_opts);
	m_config_parse_config_file(smb_conf, "sd:/apps/mplayer_ce/smb.conf");
	
	if(smb_ip==NULL || smb_share==NULL) return;

	if(smb_user==NULL) smb_user=strdup("");
	if(smb_pass==NULL) smb_pass=strdup("");
	sprintf(cad,"smb%d",number);
	if(dbg_network)
	{
	 printf("Mounting SMB Share.. ip:%s  smb_share: '%s'  device: %s ",smb_ip,smb_share,cad);		
	 if(!smbInitDevice(cad,smb_user,smb_pass,smb_share,smb_ip)) printf("error \n");
	 else printf("ok \n");
	}
	else
	  smbInitDevice(cad,smb_user,smb_pass,smb_share,smb_ip);
 
}
void trysmb()
{
	int i;
	for(i=1;i<=5;i++) mount_smb(i);

	//to force connection, I don't understand why is needed
	char cad[20];
	DIR_ITER *dp;
	for(i=1;i<=5;i++) 
	{
		dp=diropen(cad); 
		if(dp!=NULL) dirclose(dp);				
	}	
}
void SetComponentFix() {
  int fix=0;
  m_config_t *comp_conf;
	m_option_t comp_opts[] =
	{
	    {   "component_fix", &fix, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};		
	
	/* read configuration */
	comp_conf = m_config_new();
	m_config_register_options(comp_conf, comp_opts);
	m_config_parse_config_file(comp_conf, "sd:/apps/mplayer_ce/mplayer.conf"); 
   
  GX_SetComponentFix(fix);  
}

bool DebugNetwork() {
  int debug=0;
  m_config_t *comp_conf;
	m_option_t comp_opts[] =
	{
	    {   "debug_network", &debug, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};		
	
	/* read configuration */
	comp_conf = m_config_new();
	m_config_register_options(comp_conf, comp_opts);
	m_config_parse_config_file(comp_conf, "sd:/apps/mplayer_ce/mplayer.conf"); 
  
  return debug!=0;     
}


void plat_init (int *argc, char **argv[]) {
	WIIDVD_Init(); 
	VIDEO_Init();
	PAD_Init();

	AUDIO_Init(NULL);
	AUDIO_RegisterDMACallback(NULL);
	AUDIO_StopDMA();

	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);
	WPAD_SetIdleTimeout(60);

	SYS_SetResetCallback (reset_cb);
	SYS_SetPowerCallback (power_cb);
	WPAD_SetPowerButtonCallback(wpad_power_cb);


	if (!sd->startup() || !fatMountSimple("sd",sd))
	{
		GX_InitVideo();
  		log_console_init(vmode, 128);
  		printf("MPlayerCE\n");
		printf("Unofficial MPlayer v.0.21e\n\n");
		printf("Mount SD failed\n");
		VIDEO_WaitVSync();
		sleep(5);
		exit(0);
	}else fatSetReadAhead("sd:", 4, 128);
	
	SetComponentFix();
	GX_InitVideo();
	log_console_init(vmode, 128);

	printf("MPlayerCE\n");
	printf("Unofficial MPlayer v.0.3\n\n");
	printf("MPlayer/Wii port (c) 2008 Team Twiizers\n");
	printf("Used Code (c) MPlayerWii[rOn], GeeXboX,\n");
	printf("Play Media files from SD, USB, DATA-DVD, SMB & Radio Streams.\n");
	printf("Unofficial Modified MPlayer by MPlayerCE Team!\n");
	printf("Scip, Rodries, AgentX, DJDynamite123, Tipolosko, Tantric, etc.\n\n");	
	

	mainthread=LWP_GetSelf(); 
	lwp_t clientthread;
	
	dbg_network=DebugNetwork();  
	if(dbg_network)
	{
	  if(wait_for_network_initialisation()) 
	  {
		trysmb();
	  }
	  printf("Pause for reading (10 seconds)...");
	  VIDEO_WaitVSync();
		sleep(10);
	}else LWP_CreateThread(&clientthread, networkthreadfunc, NULL, NULL, 0, 80); // network initialization

	LWP_CreateThread(&clientthread, mountthreadfunc, NULL, NULL, 0, 80); // auto-mount file system
 
  
	chdir("sd:/apps/mplayer_ce");
	setenv("HOME", "sd:/apps/mplayer_ce", 1);
	setenv("DVDCSS_CACHE", "off", 1);
	//setenv("DVDCSS_VERBOSE", "2", 1);
	setenv("DVDCSS_VERBOSE", "0", 1);
	setenv("DVDREAD_VERBOSE", "0", 1);
	 
	*argv = default_args;
	*argc = sizeof(default_args) / sizeof(char *);
  
}

void plat_deinit (int rc) {
	exit_automount_thread=true;

	WIIDVD_Close();
	fatUnmount("sd");
	fatUnmount("usb");

	smbClose("smb1");
	smbClose("smb2");
	smbClose("smb3");
	smbClose("smb4");
	smbClose("smb5");

	if (power_pressed) {
		//printf("shutting down\n");
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}

	// only needed to debug problems
	/*
	log_console_enable_video(true);

	VIDEO_WaitVSync();

	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	if (rc != 0) sleep(5);
	
	log_console_deinit();
	*/
}
