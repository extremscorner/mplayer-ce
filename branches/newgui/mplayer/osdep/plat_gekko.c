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
#include <time.h>

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
#include "libdvdiso.h"


#include "log_console.h"
#include "gx_supp.h"
#include "plat_gekko.h"

#include "mload.h"
#include "ehcmodule_elf.h"

#include "../m_option.h"
#include "../parser-cfg.h"
#include "../get_path.h"

#ifndef WIILIB
#include "osdep/mem2_manager.h"
#endif

#undef abort

#define MPCE_VERSION "0.7"

extern int stream_cache_size;
  
bool reset_pressed = false;
bool power_pressed = false;
bool playing_usb = false;
bool playing_dvd = false;
bool loading_ehc = true;
int network_inited = 0;
int mounting_usb=0;
static bool dvd_mounted = false;
static bool dvd_mounting = false;
static int dbg_network = false;
//static int component_fix = false;  //deprecated
static float gxzoom=358;
static float hor_pos=3;
static float vert_pos=0;
static float stretch=0;

static bool usb_init=false;
static bool exit_automount_thread=false;
static bool net_called=false;

#define MOUNT_STACKSIZE 32768
static u8 mount_Stack[MOUNT_STACKSIZE] ATTRIBUTE_ALIGN (32);
#define NET_STACKSIZE 32768
static u8 net_Stack[NET_STACKSIZE] ATTRIBUTE_ALIGN (32);

//#define CE_DEBUG 1

static char *default_args[] = {
	"sd:/apps/mplayer_ce/mplayer.dol",
	"-bgvideo", NULL, 
	"-idle", NULL,
#ifndef CE_DEBUG 	
	"-really-quiet",
#endif	
	"-vo","gekko","-ao","gekko",
	"-menu","-menu-startup"
}; 

static void reset_cb (void) {
	reset_pressed = true;
}

static void power_cb (void) {
	power_pressed = true;
}

static void wpad_power_cb (void) {
	power_pressed = true;
}

#include <sys/time.h>
#include <sys/timeb.h>
void gekko_gettimeofday(struct timeval *tv, void *tz) {
	u64 t;
	t=gettime();
	tv->tv_sec = ticks_to_secs(t);
	tv->tv_usec = ticks_to_microsecs(t);
} 
 
void gekko_abort(void) {
	//printf("abort() called\n");
	plat_deinit(-1);
	exit(-1);
}

void __dec(char *cad){int i;for(i=0;cad[i]!='\0';i++)cad[i]=cad[i]-50;}
void sp(){sleep(5);}

void trysmb();

// Mounting code
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

const static DISC_INTERFACE* sd = &__io_wiisd;
const static DISC_INTERFACE* usb = &__io_usbstorage;

static lwp_t mainthread;

static s32 initialise_network() 
{
    s32 result;
    int cnt=0;
    net_called=true;
    //printf("initialise_network\n");
    while ((result = net_init()) == -EAGAIN) 
	{
		usleep(500);
		//printf("err net\n");
	}
    return result;
}

int wait_for_network_initialisation() 
{
	if(network_inited) return 1;
	
	while(1)
	{	
	    if (initialise_network() >= 0) {
	        char myIP[16];
	        if (if_config(myIP, NULL, NULL, true) < 0)
			{
			  if(dbg_network) 
			  {
			  	printf("Error getting ip\n");
			  	return 0;
			  }
			  sleep(5);
			  continue;
			}
	        else
			{
			  network_inited = 1;
			  if(dbg_network) printf("Network initialized. IP: %s\n",myIP);
			  return 1;
			}
	    }
	    if(dbg_network) 
		{
			printf("Error initializing network\n");
			return 0;
		}
		sleep(10);
    }
	
	return 0;
}


bool DVDGekkoMount()
{
	if(playing_dvd || dvd_mounted) return true;
	set_osd_msg(1, 1, 10000, "Mounting DVD, please wait");
	force_osd();

	//if(dvd_mounted) return true;
	dvd_mounting=true;
	if(WIIDVD_DiscPresent())
	{
		int ret;
		WIIDVD_Unmount();
		ret = WIIDVD_Mount();
		dvd_mounted=true;
		dvd_mounting=false;
		if(ret==-1) return false;
		return true;		
	}
	dvd_mounting=false;
	dvd_mounted=false;
	return false;
}

static void * networkthreadfunc (void *arg)
{	
	while(1){
		if(wait_for_network_initialisation()) 
		{
			trysmb();
			break;
		}
		sleep(10);
	}	
	LWP_JoinThread(mainthread,NULL);
	return NULL;
}

#include <sys/iosupport.h>
bool DeviceMounted(const char *device)
{
  devoptab_t *devops;
  int i,len;
  char *buf;
  
  len = strlen(device);
  buf=(char*)malloc(sizeof(char)*len+2);
  strcpy(buf,device);
  if ( buf[len-1] != ':')
  {
    buf[len]=':';  
    buf[len+1]='\0';
  }   
  devops = (devoptab_t*)GetDeviceOpTab(buf);
  if (!devops) return false;
  for(i=0;buf[i]!='\0' && buf[i]!=':';i++);  
  if (!devops || strncasecmp(buf,devops->name,i)) return false;
  return true;
}


bool mount_smb(int number)
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
	char cad[4][10];
	sprintf(cad[0],"ip%d",number);smb_opts[0].name=cad[0];	
	sprintf(cad[1],"share%d",number);smb_opts[1].name=cad[1];	
	sprintf(cad[2],"user%d",number);smb_opts[2].name=cad[2];	
	sprintf(cad[3],"pass%d",number);smb_opts[3].name=cad[3];	

	/* read configuration */
	char file[100];
	sprintf(file,"%s/smb.conf",MPLAYER_DATADIR);	

	smb_conf = m_config_new();
	m_config_register_options(smb_conf, smb_opts);
	m_config_parse_config_file(smb_conf, file);
	m_config_free(smb_conf);

	
	if(smb_ip==NULL || smb_share==NULL) 
	{
		return false;
	}

	if(smb_user==NULL) smb_user=strdup("");
	if(smb_pass==NULL) smb_pass=strdup("");
	sprintf(cad[0],"smb%d",number);
	if(dbg_network)
	{
	 printf("Mounting SMB : '%s' ip:%s  share:'%s'\n",cad[0],smb_ip,smb_share);
	 if(!smbInitDevice(cad[0],smb_user,smb_pass,smb_share,smb_ip)) 
	 {
	 	printf("error mounting '%s'\n",cad[0]);
	 	return false;
	 }
	 else 
	 {
	 	printf("ok mounting '%s'\n",cad[0]);
	 	return true;
	 }
	}
	else
	  return smbInitDevice(cad[0],smb_user,smb_pass,smb_share,smb_ip);
 
}

void trysmb()
{
	int i;
	
	if(!dbg_network)
	{
		while(loading_ehc) usleep(50000);
		usleep(50000);
	}
	for(i=1;i<=5;i++) 
	{
		usleep(1000);
		if(mount_smb(i))
		{
/*
			//to force connection, I don't understand why is needed
			char cad[20];
			
			DIR_ITER *dp;
			sprintf(cad,"smb%d",i);
			dp=diropen(cad); 
			if(dp!=NULL) dirclose(dp);				
*/			
		}
	}	
}

int LoadParams()
{
  m_config_t *comp_conf;
	m_option_t comp_opts[] =
	{
	    //{   "component_fix", &component_fix, CONF_TYPE_FLAG, 0, 0, 1, NULL},  //deprecated
	    {   "debug_network", &dbg_network, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "gxzoom", &gxzoom, CONF_TYPE_FLOAT, CONF_RANGE, 200, 500, NULL},
	    {   "hor_pos", &hor_pos, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},	  
	    {   "vert_pos", &vert_pos, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},	  
	    {   "horizontal_stretch", &stretch, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},	  
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};		
	
	/* read configuration */
	comp_conf = m_config_new();
	m_config_register_options(comp_conf, comp_opts);
	int ret;
	char cad[100];
	sprintf(cad,"%s/mplayer.conf",MPLAYER_DATADIR);
	return m_config_parse_config_file(comp_conf, cad); 
}

static bool CheckPath(char *path)
{
	char *filename;
	FILE *f;
	
	filename=malloc(sizeof(char)*(strlen(path)+15));
	strcpy(filename,path);
	strcat(filename,"/mplayer.conf");
	
	f=fopen(filename,"r");
	free(filename);
	if(f==NULL) return false;
	fclose(f);

	sprintf(MPLAYER_DATADIR,"%s",path);
	sprintf(MPLAYER_CONFDIR,"%s",path);
	sprintf(MPLAYER_LIBDIR,"%s",path);
	
	return true;
}
static bool DetectValidPath()
{
	
	if(sd->startup()) 
	{
		if(fatMount("sd",sd,0,2,256))
		{	
			if(CheckPath("sd:/apps/mplayer_ce")) return true;	
			if(CheckPath("sd:/mplayer")) return true;
		}
	}
	usb->startup();
	usb_init=true;
	//if (usb->startup())
	{
		if(fatMount("usb",usb,0,2,256))
		{
			if(CheckPath("usb:/apps/mplayer_ce")) return true;
			if(CheckPath("usb:/mplayer")) return true;
		}
	}
	return false;	
}

static bool load_echi_module()
{
	//int ret;  
	data_elf my_data_elf;
	
	//ret=mload_init();
	//if(ret<0) return false;
	
	//if(((u32) ehcmodule_elf) & 3) return false;

	mload_elf((void *) ehcmodule_elf, &my_data_elf);

	mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio);

	usleep(500);
	return true;
}

static void * mloadthreadfunc (void *arg)
{	
	while(!net_called) usleep(5000);
	int i;
	for(i=0;i<32;i++)usleep(50000);
	if(!load_echi_module()) 
	{
		//printf("usb2 cios not detected\n");
		DisableUSB2(true);
	}
	//else printf("usb2 cios detected\n");
	usleep(100);
	loading_ehc=false;
	LWP_JoinThread(mainthread,NULL);
	return NULL;
}

static void * mountthreadfunc (void *arg)
{
	int dp, dvd_inserted=0,usb_inserted=0;

#ifndef CE_DEBUG
	//sleep(1);
	while(loading_ehc || !net_called) usleep(50000);
	usleep(50000);
		
	//printf("mountthreadfunc ok\n");
		
	usb_inserted=usb_init;
	
	while(!exit_automount_thread)
	{	
		if(!playing_usb)
		{
			mounting_usb=1;
			
			dp=usb->isInserted();
			usleep(500); // needed, I don't know why, but hang if it's deleted
			//printf("usb isInserted: %d\n",dp);
			if(dp!=usb_inserted)
			{
				//printf("usb isInserted: %d\n",dp);
				usb_inserted=dp;
				if(!dp)
				{
					//printf("unmount usb\n");
					fatUnmount("usb:");
				}else 
				{
					//printf("mount usb\n");
					fatMount("usb",usb,0,2,256);
				}
			}
			mounting_usb=0;
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
		//usleep(300000);		
		sleep(1);
	}
#endif	
	LWP_JoinThread(mainthread,NULL);
	return NULL;
}

bool FindIOS(u32 ios)
{
	u32 len_buf;
	s32 ret;
	int n;

	u64 *titles = NULL;
	u32 num_titles=0;

	ret = ES_GetNumTitles(&num_titles);
	if (ret < 0)
	{
		printf("error ES_GetNumTitles\n");
		return false;
	}

	if(num_titles<1) 
	{
		printf("error num_titles<1\n");
		return false;
	}

	titles = memalign(32, num_titles * sizeof(u64) + 32);
	if (!titles)
	{
		printf("error memalign\n");
		return false;
	}

	ret = ES_GetTitles(titles, num_titles);
	if (ret < 0)
	{
		free(titles);
		printf("error ES_GetTitles\n");
		return false;	
	}
		
	for(n=0; n<num_titles; n++) {
		u32 tidl = (titles[n] &  0xFFFFFFFF);
		if((titles[n] &  0xFFFFFFFF)==ios) 
		{
			free(titles); 
			return true;
		}
	}
	
    free(titles); 
	return false;

}

#ifdef WIILIB
void plat_init (int *argc, char **argv[])
{
	//log_console_init(vmode, 0);
	GX_SetCamPosZ(gxzoom);
	GX_SetScreenPos((int)hor_pos,(int)vert_pos,(int)stretch);
	DetectValidPath();

	//chdir(MPLAYER_DATADIR);
	setenv("HOME", MPLAYER_DATADIR, 1);
	setenv("DVDCSS_CACHE", "off", 1);
	setenv("DVDCSS_VERBOSE", "0", 1);
	setenv("DVDREAD_VERBOSE", "0", 1);
	setenv("DVDCSS_RAW_DEVICE", "/dev/di", 1);

	stream_cache_size=8*1024; //default cache size (8MB)
}

#else
void plat_init (int *argc, char **argv[]) {	
	int mload;
	
	if(IOS_GetVersion()!=202)
	{
		if(FindIOS(202)) 
		{
			IOS_ReloadIOS(202);
			WIIDVD_Init(false);
		}
		else WIIDVD_Init(true);
	} 
	else WIIDVD_Init(false);
//	IOS_ReloadIOS(202);
//	if(IOS_GetVersion()<200) IOS_ReloadIOS(202);

	mload=mload_init();

	InitMem2Manager();

	VIDEO_Init();

	PAD_Init();
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);
	WPAD_SetIdleTimeout(60);

	WPAD_SetPowerButtonCallback(wpad_power_cb);
	
	AUDIO_Init(NULL);
	AUDIO_RegisterDMACallback(NULL);
	AUDIO_StopDMA();


	if (!DetectValidPath())
	{
		GX_InitVideo();
		log_console_init(vmode, 0);
		printf("MPlayerCE v.%s\n\n",MPCE_VERSION);
		printf("SD/USB access failed\n");
		printf("Please check that you have installed MPlayerCE in the right folder\n");
		printf("Valid folders:\n");
		printf(" sd:/apps/mplayer_ce\n sd:/mplayer\n usb:/apps/mplayer_ce\n usb:/mplayer\n");
				
		VIDEO_WaitVSync();
		sleep(6);
		if (!*((u32*)0x80001800)) SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
		exit(0);
	}
	SYS_SetResetCallback (reset_cb);
	SYS_SetPowerCallback (power_cb);

	LoadParams();
	//GX_SetComponentFix(component_fix); //deprecated
	GX_SetCamPosZ(gxzoom);
	GX_SetScreenPos((int)hor_pos,(int)vert_pos,(int)stretch);  

	GX_InitVideo();

	//log_console_init(vmode, 128);
	log_console_init(vmode, 0);

  printf("Loading ");
  
  char cad[10]={127,130,158,147,171,151,164,117,119,0};
  __dec(cad);
  printf ("\x1b[32m");
	printf("%s",cad);
	printf(" v.%s ....\n\n",MPCE_VERSION);
  printf ("\x1b[37m");


	VIDEO_WaitVSync();

	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

//usb->startup(); //init usb before network (testing)

	mainthread=LWP_GetSelf(); 
	lwp_t clientthread;
	 
#ifndef CE_DEBUG  //no network on debug

	if(dbg_network)
	{
		printf("\nDebugging Network\n");
		if(wait_for_network_initialisation()) 
		{
			trysmb();
		}
		printf("Pause for reading (10 seconds)...");
		VIDEO_WaitVSync();
		sleep(10);
	}
	else 
	{
		LWP_CreateThread(&clientthread, networkthreadfunc, NULL, net_Stack, NET_STACKSIZE, 80); // network initialization
		usleep(1000);
	}

#endif
	if(usb_init)
	{
		usleep(1000);
		if(mload<0) 
		{
			DisableUSB2(true);
			loading_ehc=false;
		}
		else
		{		
			fatUnmount("usb:");
		 	load_echi_module();
			//usleep(1000);
			fatMount("usb",usb,0,2,256);
		}
	}
	else 
	{
		if(mload<0) 
		{
			DisableUSB2(true);
			loading_ehc=false;
		}
		else LWP_CreateThread(&clientthread, mloadthreadfunc, NULL, NULL, 0, 80); // for fix network initialization
	}

	chdir(MPLAYER_DATADIR);
	setenv("HOME", MPLAYER_DATADIR, 1);
	setenv("DVDCSS_CACHE", "off", 1);
	setenv("DVDCSS_VERBOSE", "0", 1);
	setenv("DVDREAD_VERBOSE", "0", 1);
	setenv("DVDCSS_RAW_DEVICE", "/dev/di", 1);



	default_args[2]=malloc(sizeof(char)*strlen(MPLAYER_DATADIR)+16);
	strcpy(default_args[2],MPLAYER_DATADIR);
	default_args[4]=malloc(sizeof(char)*strlen(MPLAYER_DATADIR)+16);
	strcpy(default_args[4],MPLAYER_DATADIR);

	if (CONF_GetAspectRatio()) 
	{ //16:9
		strcat(default_args[2],"/loop-wide.avi");
		strcat(default_args[4],"/loop-wide.avi");
	}		
	else
	{  // 4:3
		strcat(default_args[2],"/loop.avi");
		strcat(default_args[4],"/loop.avi");
	}
	
	*argv = default_args;
	*argc = sizeof(default_args) / sizeof(char *);


	stream_cache_size=8*1024; //default cache size (8MB)
		
	if (!*((u32*)0x80001800)) sp();
	
	//usleep(500);
	LWP_CreateThread(&clientthread, mountthreadfunc, NULL, mount_Stack, MOUNT_STACKSIZE, 80); // auto mount fs (usb, dvd)
	//usleep(1000);
	log_console_enable_video(false);
	
}
#endif

void plat_deinit (int rc) {

	exit_automount_thread=true;
	usleep(2000);
	if (power_pressed) {
		//printf("shutting down\n");
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	//log_console_enable_video(true);
	//printf("exiting mplayerce\n");sleep(3);
	//log_console_deinit();
}

#ifdef WIILIB
struct SMBSettings {
	char	ip[16];
	char	share[20];
	char	user[20];
	char	pwd[20];
};
struct SMBSettings smbConf[5];

/****************************************************************************
 * LoadConfig
 *
 * Loads all MPlayer .conf files
 ***************************************************************************/
void LoadConfig(char * path)
{
	int i;
	char filepath[1024];
	char tmp[20];
	char* smb_ip;
	char* smb_share;
	char* smb_user;
	char* smb_pass;

	// mplayer.conf
	m_config_t *comp_conf;
	m_option_t comp_opts[] =
	{
	//{   "component_fix", &component_fix, CONF_TYPE_FLAG, 0, 0, 1, NULL},  //deprecated
	{   "debug_network", &dbg_network, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{   "gxzoom", &gxzoom, CONF_TYPE_FLOAT, CONF_RANGE, 200, 500, NULL},
	{   "hor_pos", &hor_pos, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},
	{   "vert_pos", &vert_pos, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},
	{   "horizontal_stretch", &stretch, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},
	{   NULL, NULL, 0, 0, 0, 0, NULL }
	};

	comp_conf = m_config_new();
	m_config_register_options(comp_conf, comp_opts);
	sprintf(filepath,"%s/mplayer.conf", path);
	m_config_parse_config_file(comp_conf, filepath);

	// smb.conf
	memset(&smbConf, 0, sizeof(smbConf));

	m_config_t *smb_conf;
	m_option_t smb_opts[] =
	{
		{   NULL, &smb_ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
		{   NULL, &smb_share, CONF_TYPE_STRING, 0, 0, 0, NULL },
		{   NULL, &smb_user, CONF_TYPE_STRING, 0, 0, 0, NULL },
		{   NULL, &smb_pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
		{   NULL, NULL, 0, 0, 0, 0, NULL }
	};

	for(i=1; i<=5; i++)
	{
		smb_ip = NULL;
		smb_share = NULL;
		smb_user = NULL;
		smb_pass = NULL;

		sprintf(tmp,"ip%d",i); smb_opts[0].name=strdup(tmp);
		sprintf(tmp,"share%d",i); smb_opts[1].name=strdup(tmp);
		sprintf(tmp,"user%d",i); smb_opts[2].name=strdup(tmp);
		sprintf(tmp,"pass%d",i); smb_opts[3].name=strdup(tmp);

		smb_conf = m_config_new();
		m_config_register_options(smb_conf, smb_opts);
		sprintf(filepath,"%s/smb.conf", path);
		m_config_parse_config_file(smb_conf, filepath);

		if(smb_ip!=NULL && smb_share!=NULL)
		{
			if(smb_user==NULL) smb_user=strdup("");
			if(smb_pass==NULL) smb_pass=strdup("");

			snprintf(smbConf[i-1].ip, 16, "%s", smb_ip);
			snprintf(smbConf[i-1].share, 20, "%s", smb_share);
			snprintf(smbConf[i-1].user, 20, "%s", smb_user);
			snprintf(smbConf[i-1].pwd, 20, "%s", smb_pass);
		}
	}

	sprintf(MPLAYER_DATADIR,"%s",path);
	sprintf(MPLAYER_CONFDIR,"%s",path);
	sprintf(MPLAYER_LIBDIR,"%s",path);
}
#endif