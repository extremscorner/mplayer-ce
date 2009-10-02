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
#include <malloc.h>


#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/lwp.h>
#include <debug.h>
#include <wiiuse/wpad.h>

#include <fat.h>
#include <ntfs.h>
#include <smb.h>
#include <ftp.h> 

#include <network.h>
#include <errno.h>
#include <di/di.h>
#include "libdvdiso.h"
#include "mp_osd.h"

#include "log_console.h"
#include "gx_supp.h"
#include "plat_gekko.h"

#include "mload.h"
#include "ehcmodule_elf.h"

#include "../m_option.h"
#include "../parser-cfg.h"
#include "../get_path.h"

#undef abort

#define MPCE_VERSION "0.76"

extern int stream_cache_size;

static float gxzoom=348;
static float hor_pos=3;
static float vert_pos=0;
static float stretch=0;

static off_t get_filesize(char *FileName)
{
    struct stat file;
    if(!stat(FileName,&file))
    {
        return file.st_size;
    }
    return 0;
}

bool load_ehci_module()
{
	data_elf my_data_elf;
	off_t fsize;
	void *external_ehcmodule = NULL;
	FILE *fp;
	char file[100];
	
	sprintf(file,"%s/ehcmodule.elf",MPLAYER_DATADIR);	
	
	fp=fopen(file,"rb");
	if(fp!=NULL)
	{
		fsize=get_filesize(file);
		external_ehcmodule= (void *)memalign(32, fsize);
		if(!external_ehcmodule) 
		{
			fclose(fp);
			free(external_ehcmodule); 
			external_ehcmodule=NULL;
		}
		else
		{
			if(fread(external_ehcmodule,1, fsize ,fp)!=fsize)
			{
				free(external_ehcmodule); 
				external_ehcmodule=NULL;
			}
			else mload_elf((void *) external_ehcmodule, &my_data_elf);
			fclose(fp);
		}
		
	}
	else
		mload_elf((void *) ehcmodule_elf, &my_data_elf);

	if(mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio)<0)
	{
		if(mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, 0x48)<0) return false;
	}
	usleep(1000);
	return true;
}

bool FindIOS(u32 ios)
{
	//u32 len_buf;
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

	titles = (u64 *)memalign(32, num_titles * sizeof(u64) + 32);
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
		//u32 tidl = (titles[n] &  0xFFFFFFFF);
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


	//fixed at now, I think this path has to be passed by gui
	sprintf(MPLAYER_DATADIR,"%s","sd:/apps/mplayer_ce");
	sprintf(MPLAYER_CONFDIR,"%s","sd:/apps/mplayer_ce");
	sprintf(MPLAYER_LIBDIR,"%s","sd:/apps/mplayer_ce");
	chdir(MPLAYER_DATADIR);
	
	setenv("HOME", MPLAYER_DATADIR, 1);
	setenv("DVDCSS_CACHE", "off", 1);
	setenv("DVDCSS_VERBOSE", "0", 1);
	setenv("DVDREAD_VERBOSE", "0", 1);
	setenv("DVDCSS_RAW_DEVICE", "/dev/di", 1);

	stream_cache_size=8*1024; //default cache size (8MB)
}
void plat_deinit (int rc)
{

}
#else

#include "osdep/mem2_manager.h"

//#define CE_DEBUG 1
#define USE_NET_THREADS
  
bool reset_pressed = false;
bool power_pressed = false;
bool playing_usb = false;
bool playing_dvd = false;
//bool loading_ehc = true;
int network_inited = 0;
int mounting_usb=0;
static bool dvd_mounted = false;
static bool dvd_mounting = false;
static bool dbg_network = false;
//static int component_fix = false;  //deprecated

static bool usb_init=false;
static bool exit_automount_thread=false;
//static bool net_called=false;
lwp_t mountthread;

#define MOUNT_STACKSIZE 8*1024
static u8 mount_Stack[MOUNT_STACKSIZE] ATTRIBUTE_ALIGN (32);
#define NET_STACKSIZE 8*1024
static u8 net_Stack[NET_STACKSIZE] ATTRIBUTE_ALIGN (32);
#ifdef USE_NET_THREADS
#define CONN_STACKSIZE 8*1024
static u8 smbx_Stack[5][CONN_STACKSIZE] ATTRIBUTE_ALIGN (32);	
static u8 ftpx_Stack[5][CONN_STACKSIZE] ATTRIBUTE_ALIGN (32);	
#endif

#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

const static DISC_INTERFACE* sd = &__io_wiisd;
const static DISC_INTERFACE* usb = &__io_usbstorage;

static lwp_t mainthread;

bool mount_sd_ntfs()
{
	//only mount the first ntfs partition
	int partition_count = 0;
	sec_t *partitions = NULL;
	sec_t boot;
	
	partition_count = ntfsFindPartitions (sd, &partitions);
	if(partition_count<1) return 0;

	boot=partitions[0];
	free(partitions);
	
	return ntfsMount("ntfs_sd", sd, boot, 256, 4, NTFS_DEFAULT | NTFS_RECOVER ) ;
}

bool mount_usb_ntfs()
{
	//only mount the first ntfs partition
	int partition_count = 0;
	sec_t *partitions = NULL;
	sec_t boot;
	
	partition_count = ntfsFindPartitions (usb, &partitions);
	if(partition_count<1) return 0;
	boot=partitions[0];
	free(partitions);
	return ntfsMount("ntfs_usb", usb, boot, 256, 4, NTFS_DEFAULT | NTFS_RECOVER ) ;
}

static void * mountthreadfunc (void *arg)
{
	int dp, dvd_inserted=0,usb_inserted=0;
//todo: add sd automount
#ifndef CE_DEBUG
	sleep(1);
	mount_sd_ntfs(); //only once now
		
	usb_inserted=usb_init;
	
	while(!exit_automount_thread)
	{	
		if(!playing_usb)
		{
			mounting_usb=1;
			
			dp=usb->isInserted();
			usleep(500); // needed, I don't know why, but hang if it's deleted
			//printf(".");fflush(stdout);
			if(dp!=usb_inserted)
			{
				//printf("usb isInserted: %d\n",dp);
				usb_inserted=dp;
				if(!dp)
				{
					//printf("unmount usb\n");
					fatUnmount("usb:");
					ntfsUnmount ("ntfs_usb", true);
				}else 
				{
					//printf("mount usb\n");
					fatMount("usb",usb,0,3,256);
					mount_usb_ntfs();
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
		if(exit_automount_thread) break;
		usleep(200000);	
		if(exit_automount_thread) break;	
		usleep(200000);		
		if(exit_automount_thread) break;	
		usleep(200000);		
		if(exit_automount_thread) break;	
		usleep(200000);		
		//usleep(300000);		
		//sleep(1);
	}
#endif
	return NULL;
}


static char *default_args[] = {
	"sd:/apps/mplayer_ce/mplayer.dol",
	"-bgvideo", NULL, 
	"-idle", NULL,
#ifndef CE_DEBUG 	
	"-really-quiet",
//	"-msglevel","all=9",
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

static void wpad_power_cb (s32 chan) {
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

static void __dec(char *cad){int i;for(i=0;cad[i]!='\0';i++)cad[i]=cad[i]-50;}
static void sp(){sleep(5);}


/******************************************/
/*           NETWORK FUNCTIONS            */
/******************************************/

static s32 initialise_network() 
{
    s32 result;
    while ((result = net_init()) == -EAGAIN) 
	{
		usleep(1000);
	}
    return result;
}

static int wait_for_network_initialisation() 
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
			  if(dbg_network) printf("Network initialized. IP: %s\n",myIP);
			  usleep(1000);
			  network_inited = 1;
			  return 1;
			}
	    }
	    if(dbg_network) 
		{
			printf("Error initializing network\n");
			return 0;
		}
		sleep(5);
    }
	
	return 0;
}

static void trysmb();
static void tryftp();

static void * networkthreadfunc (void *arg)
{	
	wait_for_network_initialisation();
	usleep(500);
#ifndef USE_NET_THREADS	
	trysmb();
	tryftp();
#endif	
	return NULL;
}

static bool mount_smb(int number)
{	
	char* smb_ip=NULL;
	char* smb_share=NULL;
	char* smb_user=NULL;
	char* smb_pass=NULL;
	char device[10];
	char file[100];

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

	sprintf(device,"smb%d",number);

	/* read configuration */
	sprintf(file,"%s/smb.conf",MPLAYER_DATADIR);	

	smb_conf = m_config_new();
	m_config_register_options(smb_conf, smb_opts);
	if(m_config_parse_config_file(smb_conf, file)==0)
	{
		m_config_free(smb_conf);
		usleep(1000);
		return false;
	}
	m_config_free(smb_conf);

	if(smb_ip==NULL || smb_share==NULL) 
	{
		if(dbg_network) printf("SMB %s not filled\n",device);
		sleep(1); // sync problem on libogc threads
		return false;
	}

	if(smb_user==NULL) smb_user=strdup("");
	if(smb_pass==NULL) smb_pass=strdup("");
	
	if(dbg_network)
	{
		 u64 t1,t2;
	 t1=ticks_to_millisecs(gettime());

	 printf("Mounting SMB : '%s' ip:%s  share:'%s'\n",device,smb_ip,smb_share);
	 if(!smbInitDevice(device,smb_user,smb_pass,smb_share,smb_ip)) 
	 {
		t2=ticks_to_millisecs(gettime());
	 	printf("error mounting '%s' (%u ms)\n",device,(unsigned)(t2-t1));
	 	return false;
	 }
	 else 
	 {
	 	t2=ticks_to_millisecs(gettime());
	 	printf("ok mounting '%s' (%u ms)\n",device,(unsigned)(t2-t1));
	 	return true;
	 }
	}
	else
	  return smbInitDevice(device,smb_user,smb_pass,smb_share,smb_ip);
 
}

bool mount_ftp(int number)
{	
	char* ftp_ip=NULL;
	char* ftp_share=NULL;
	char* ftp_user=NULL;
	char* ftp_pass=NULL;
	int ftp_passive = false;
	char device[10];
	char cad[5][12];
	char file[100];	
	m_config_t *ftp_conf;
	m_option_t ftp_opts[] =
	{
	    {   NULL, &ftp_ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   NULL, &ftp_share, CONF_TYPE_STRING, 0, 0, 0, NULL },    
	    {   NULL, &ftp_user, CONF_TYPE_STRING, 0, 0, 0, NULL }, 
	    {   NULL, &ftp_pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   NULL, &ftp_passive, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};
	
	sprintf(device,"ftp%d",number);

	sprintf(cad[0],"ip%d",number);ftp_opts[0].name=cad[0];	
	sprintf(cad[1],"share%d",number);ftp_opts[1].name=cad[1];	
	sprintf(cad[2],"user%d",number);ftp_opts[2].name=cad[2];	
	sprintf(cad[3],"pass%d",number);ftp_opts[3].name=cad[3];	
	sprintf(cad[4],"passive%d",number);ftp_opts[4].name=cad[4];	
	/* read configuration */
	sprintf(file,"%s/ftp.conf",MPLAYER_DATADIR);	
	
	ftp_conf = m_config_new();
	m_config_register_options(ftp_conf, ftp_opts);
	if(m_config_parse_config_file(ftp_conf, file)==0)
	{
		m_config_free(ftp_conf);
		usleep(1000);
		return false;
	}
	m_config_free(ftp_conf);

	if(ftp_ip==NULL || ftp_share==NULL) 
	{
		if(dbg_network) printf("FTP %s not filled\n",device);
		usleep(20000);  // sync problem on libogc threads
		return false;
	}

	if(ftp_user==NULL) ftp_user=strdup("anonymous");
	if(ftp_pass==NULL) ftp_pass=strdup("anonymous");
	
	if(dbg_network)
	{
	 printf("Mounting FTP : '%s' host:%s  share:'%s, PASV: %d'\n",device,ftp_ip,ftp_share,ftp_passive);
	 u64 t1,t2;
	 t1=GetTimerMS();
	 if(!ftpInitDevice(device,ftp_user,ftp_pass,ftp_share,ftp_ip,ftp_passive>0)) 
	 { 
		 t2=GetTimerMS()-t1;
	 	printf("error mounting '%s' (%u ms)\n",device,(unsigned)(t2));
	 	return false;
	 }
	 else 
	 {
	 	t2=GetTimerMS()-t1;
	 	printf("ok mounting '%s' (%u ms)\n",device,(unsigned)(t2));
	 	return true;
	 }
	}
	else
	  return ftpInitDevice(device,ftp_user,ftp_pass,ftp_share,ftp_ip,ftp_passive>0); 
}

static void trysmb()
{
	int i;
	for(i=1;i<=5;i++) mount_smb(i);
}

static void tryftp()
{	
	int i;
	for(i=1;i<=5;i++) mount_ftp(i);
}

#ifdef USE_NET_THREADS		
static void * smbthread (void *arg)
{
	int i;
	i=*((int*)arg);	
	while(network_inited==0) usleep(5000);
	usleep(1000);
	mount_smb(i+1);
	
	return NULL;
}

static void * ftpthread (void *arg)
{
	int i;
	i=*((int*)arg);	
	while(network_inited==0) usleep(5000);
	usleep(2000);
	mount_ftp(i+1);
	
	return NULL;
}
#endif
	
static void InitNetworkThreads()
{
#ifdef USE_NET_THREADS
	int i,x1[5],x2[5];
#endif	
	lwp_t clientthread;		
	memset (net_Stack, 0, NET_STACKSIZE);
	

	LWP_CreateThread(&clientthread, networkthreadfunc, NULL, net_Stack, NET_STACKSIZE, 64); // network initialization
#ifdef USE_NET_THREADS
	for(i=0;i<5;i++) 
	{
		x1[i]=i;
		memset (ftpx_Stack[i], 0, CONN_STACKSIZE);
		LWP_CreateThread(&clientthread, ftpthread, &x1[i], ftpx_Stack[i], CONN_STACKSIZE, 64); // ftp initialization
		usleep(100);
	} 
	for(i=0;i<5;i++) 
	{
		x2[i]=i;
		memset (smbx_Stack[i], 0, CONN_STACKSIZE);
		LWP_CreateThread(&clientthread, smbthread, &x2[i], smbx_Stack[i], CONN_STACKSIZE, 64); // samba initialization
		usleep(100);
	} 
#endif	
}
/******************************************/
/*        END NETWORK FUNCTIONS           */
/******************************************/

bool DVDGekkoMount()
{
	if(playing_dvd || dvd_mounted) return true;
	set_osd_msg(OSD_MSG_TEXT, 1, 5000, "Mounting DVD, please wait");
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




static int LoadParams()
{
	char cad[100];
	m_config_t *comp_conf;
	m_option_t comp_opts[] =
	{
	    //{   "component_fix", &component_fix, CONF_TYPE_FLAG, 0, 0, 1, NULL},  //deprecated
	    {   "debug_network", &dbg_network, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "gxzoom", &gxzoom, CONF_TYPE_FLOAT, CONF_RANGE, 200, 500, NULL},
	    {   "hor_pos", &hor_pos, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},	  
	    {   "vert_pos", &vert_pos, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},	  
	    {   "horizontal_stretch", &stretch, CONF_TYPE_FLOAT, CONF_RANGE, -400, 400, NULL},
		{	"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 32, 1048576, NULL},	 
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};		
	
	/* read configuration */
	comp_conf = m_config_new();
	m_config_register_options(comp_conf, comp_opts);
	
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
		if(fatMount("sd",sd,0,3,256))
		{	
			if(CheckPath("sd:/apps/mplayer_ce")) return true;	
			if(CheckPath("sd:/mplayer")) return true;
		}
		else if(mount_sd_ntfs())
		{
			if(CheckPath("ntfs_sd:/apps/mplayer_ce")) return true;	
			if(CheckPath("ntfs_sd:/mplayer")) return true;
		}

	}
	usb->startup();
	usb_init=true;
	//if (usb->startup())
	{
		if(fatMount("usb",usb,0,3,256))
		{
			if(CheckPath("usb:/apps/mplayer_ce")) return true;
			if(CheckPath("usb:/mplayer")) return true;
		}
		else if(mount_usb_ntfs())
		{
			if(CheckPath("ntfs_usb:/apps/mplayer_ce")) return true;	
			if(CheckPath("ntfs_usb:/mplayer")) return true;
		}
	}
	return false;	
}

void show_mem()
{
printf("m1(%.2f) m2(%.2f)\n",
						 	((float)((char*)SYS_GetArenaHi()-(char*)SYS_GetArenaLo()))/0x100000,
							 ((float)((char*)SYS_GetArena2Hi()-(char*)SYS_GetArena2Lo()))/0x100000);

}

void plat_init (int *argc, char **argv[]) {	
	int mload=-1;
	char cad[10]={127,130,158,147,171,151,164,117,119,0};
	
	VIDEO_Init();
	GX_InitVideo();
	log_console_init(vmode, 0);

	printf("Loading ");
  
	__dec(cad);
	printf("\x1b[32m%s v.%s ....\x1b[37m\n\n",cad,MPCE_VERSION);	
	
	VIDEO_WaitVSync();

	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	
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
	
	mload=mload_init();

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

	stream_cache_size=8*1024; //default cache size (8MB)
	LoadParams();
	//GX_SetComponentFix(component_fix); //deprecated
	GX_SetCamPosZ(gxzoom);
	GX_SetScreenPos((int)hor_pos,(int)vert_pos,(int)stretch);  


	mainthread=LWP_GetSelf(); 	
	 
#ifndef CE_DEBUG  //no network on debug
	if(dbg_network)
	{
		printf("\nDebugging Network\n");
		if(wait_for_network_initialisation()) 
		{
			trysmb();
			tryftp();
		}
		printf("Pause for reading (10 seconds)...");
		VIDEO_WaitVSync();
		sleep(10);
	}
	else 
	{
		InitNetworkThreads();
	}
#endif
	
	if(usb_init)
	{		
		if(mload<0) 
		{
			//DisableUSB2(true);
		}
		else
		{		
			fatUnmount("usb:");
		 	load_ehci_module();
		 	usb->isInserted();
			fatMount("usb",usb,0,3,256);
			mount_usb_ntfs();
		}
	}
	else 
	{
		if(mload<0) 
		{
			//DisableUSB2(true);
		}
		else
		{
			if(!load_ehci_module()) 
			{
				//DisableUSB2(true);
			}
		}
	}

	chdir(MPLAYER_DATADIR);
	setenv("HOME", MPLAYER_DATADIR, 1);
	setenv("DVDCSS_CACHE", "off", 1);
	setenv("DVDCSS_VERBOSE", "0", 1);
	setenv("DVDREAD_VERBOSE", "0", 1);
	setenv("DVDCSS_RAW_DEVICE", "/dev/di", 1);

if(*argc<3)
{
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

}
else
{	
	if(usb->isInserted()) 
	{
		usb_init=true;
		fatMount("usb",usb,0,3,256);
		mount_usb_ntfs();
	}
}
	LWP_CreateThread(&mountthread, mountthreadfunc, NULL, mount_Stack, MOUNT_STACKSIZE, 64); // auto mount fs (usb, dvd)

	// only used for cache_mem at now  (stream_cache_size + 8kb(paranoid)
	InitMem2Manager((stream_cache_size*1024)+(8*1024));

	if (!*((u32*)0x80001800)) sp();
	//log_console_enable_video(false);
}

void plat_deinit (int rc) 
{
	exit_automount_thread=true;
	LWP_JoinThread(mountthread,NULL);
	ntfsUnmount ("ntfs0", true); //I think that we don't need it
	//ExitTimer();
	
	if (power_pressed) {
		//printf("shutting down\n");
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	if (!*((u32*)0x80001800)) SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
	//log_console_enable_video(true);
	//printf("exiting mplayerce\n");sleep(3);
	//log_console_deinit();
}

#if 0 // change 0 by 1 if you are using devkitppc r17
int _gettimeofday_r(struct _reent *ptr,	struct timeval *ptimeval ,	void *ptimezone)
{
	u64 t;
	t=gettime();
	if(ptimeval!=NULL)
	{
		ptimeval->tv_sec = ticks_to_secs(t);
		ptimeval->tv_usec = ticks_to_microsecs(t);
	}
} 
#endif

#endif
