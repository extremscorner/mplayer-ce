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

#include <fat.h>
#include <ntfs.h>
#include <ext2.h>
#include <smb.h>
#include "fst.h"
#include "gcfst.h"
#include "iso.h"

#include <network.h>
#include <errno.h>
#include "mp_osd.h"
#include "timer.h"
#include "version.h"

#include "ftp_devoptab.h"
#include "log_console.h"
#include "gx_supp.h"
#include "plat_gekko.h"

#include "mload.h"
#include "ehcmodule_elf.h"

#include "../m_option.h"
#include "../parser-cfg.h"
#include "../path.h"

#undef abort


//#define DEBUG_INIT

#ifdef DEBUG_INIT
#define printf_debug(fmt, args...) \
	printf(fmt, ##args)
#else
#define printf_debug(fmt, args...)
#endif // DEBUG_INIT


extern char appPath[1024];
extern int enable_restore_points;

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
		usleep(1000);
		if(mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, 0x47)<0)
		{
			printf_debug("ehcmodule not loaded\n");
			return false;
		}else printf_debug("ehcmodule loaded with priority: %i\n",0x47);
	}else printf_debug("ehcmodule loaded with priority: %i\n",my_data_elf.prio);
	//usleep(5000);

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

static bool hbc_stub()
{
	char * sig = (char *)0x80001804;
    if(
	   sig[0] == 'S' &&
	   sig[1] == 'T' &&
	   sig[2] == 'U' &&
	   sig[3] == 'B' &&
	   sig[4] == 'H' &&
	   sig[5] == 'A' &&
	   sig[6] == 'X' &&
	   sig[7] == 'X') return true;
    return false;
}

bool reset_pressed = false;
bool power_pressed = false;

#define WATCHDOG_STACKSIZE 8*1024
static u8 watchdog_Stack[WATCHDOG_STACKSIZE] ATTRIBUTE_ALIGN (32);
lwp_t watchdogthread;


mutex_t watchdogmutex=LWP_MUTEX_NULL;
int watchdogcounter=-1;
static int enable_watchdog=0;
static bool exit_watchdog_thread=false;

static void * watchdogthreadfunc (void *arg)
{
	long sleeptime;
	while(!exit_watchdog_thread)
	{
		sleeptime = 1000*1000; // 1 sec
		while(sleeptime > 0)
		{
			if(exit_watchdog_thread)
				return NULL;
			usleep(100);
			sleeptime -= 100;
		}

		if(exit_watchdog_thread)break;
		if(reset_pressed || power_pressed)
		{
			sleeptime = 5*1000*1000; //mplayer has 5 secs to do a clean exit
			while(sleeptime > 0)
			{
				if(exit_watchdog_thread)
					return NULL;
				usleep(100);
				sleeptime -= 100;
			}

			if(reset_pressed)
			{
				printf("reset\n");
				if (!hbc_stub()) SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
				exit(0);
			}
			if (power_pressed)
			{
				printf("power off\n");
				SYS_ResetSystem(SYS_POWEROFF, 0, 0);
				exit(0);
			}
		}
		if(watchdogcounter>=0)
		{
			if(watchdogcounter==0)
			{
				printf("timeout: return to loader\n");
				if (!hbc_stub()) SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
				exit(0);
			}
			if(watchdogmutex!=LWP_MUTEX_NULL)
			{
				LWP_MutexLock(watchdogmutex);
				watchdogcounter--;
				LWP_MutexUnlock(watchdogmutex);
			}
		}
	}
	return NULL;
}


//bool loading_ehc = true;
int network_initied = 0;
static mutex_t dvd_mutex = LWP_MUTEX_NULL;
u64 dvd_lasttick = 0;
static int dbg_network = false;
static int overscan = true;

static bool usb_init=false;
static bool exit_automount_thread=false;
//static bool net_called=false;
static int video_mode=0;
lwp_t mountthread;

#define MOUNT_STACKSIZE 8*1024
static u8 mount_Stack[MOUNT_STACKSIZE] ATTRIBUTE_ALIGN (32);

#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>
#include <di/di.h>

const static DISC_INTERFACE* sd = &__io_wiisd;
const static DISC_INTERFACE* usb = &__io_usbstorage;
const static DISC_INTERFACE* dvd = &__io_wiidvd;
const static DISC_INTERFACE* carda = &__io_gcsda;
const static DISC_INTERFACE* cardb = &__io_gcsdb;

typedef struct _PARTITION_RECORD {
	u8 status;
	u8 chs_start[3];
	u8 type;
	u8 chs_end[3];
	u32 lba_start;
	u32 block_count;
} ATTRIBUTE_PACKED PARTITION_RECORD;

typedef struct _MASTER_BOOT_RECORD {
	u8 code_area[446];
	PARTITION_RECORD partitions[4];
	u16 signature;
} ATTRIBUTE_PACKED MASTER_BOOT_RECORD;

#define MBR_SIGNATURE	0x55AA

#define PARTITION_TYPE_FREE				0x00
#define PARTITION_TYPE_FAT12			0x01
#define PARTITION_TYPE_FAT16_32MB		0x04
#define PARTITION_TYPE_FAT16			0x06
#define PARTITION_TYPE_NTFS				0x07
#define PARTITION_TYPE_FAT32			0x0b
#define PARTITION_TYPE_FAT32_LBA		0x0c
#define PARTITION_TYPE_FAT16_LBA		0x0e
#define PARTITION_TYPE_LINUX			0x83

#include "mpbswap.h"

enum {
	DEVICE_CARDA = 0,
	DEVICE_CARDB,
	DEVICE_DVD,
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_MAX,
};

static bool isInserted[DEVICE_MAX];

static void mountproc()
{
	if (isInserted[DEVICE_SD]) {
		if (!sd->isInserted()) {
			fatUnmount("sd:");
			sd->shutdown();
			isInserted[DEVICE_SD] = false;
		}
	} else if (sd->startup() && sd->isInserted()) {
		fatMount("sd", sd, 0, 2, 128);
		isInserted[DEVICE_SD] = true;
	}
	
	if (isInserted[DEVICE_USB]) {
		if (!usb->isInserted()) {
			fatUnmount("usb:");
			ntfsUnmount("ntfs", true);
			ext2Unmount("ext2");
			usb->shutdown();
			isInserted[DEVICE_USB] = false;
		}
	} else if (usb->startup() && usb->isInserted()) {
		MASTER_BOOT_RECORD mbr;
		
		if (usb->readSectors(0, 1, &mbr) && (mbr.signature == MBR_SIGNATURE)) {
			for (int i = 0; i < 4; i++) {
				PARTITION_RECORD *partition = &mbr.partitions[i];
				sec_t sector = le2me_32(partition->lba_start);
				
				switch (partition->type) {
					case PARTITION_TYPE_FREE:
						continue;
					case PARTITION_TYPE_FAT12:
					case PARTITION_TYPE_FAT16_32MB:
					case PARTITION_TYPE_FAT16:
					case PARTITION_TYPE_FAT32:
					case PARTITION_TYPE_FAT32_LBA:
					case PARTITION_TYPE_FAT16_LBA:
					{
						fatMount("usb", usb, sector, 2, 128);
					}
					case PARTITION_TYPE_NTFS:
						ntfsMount("ntfs", usb, sector, 2, 128, NTFS_DEFAULT | NTFS_RECOVER | NTFS_READ_ONLY);
					case PARTITION_TYPE_LINUX:
						ext2Mount("ext2", usb, sector, 2, 128, EXT2_FLAG_64BITS | EXT2_FLAG_JOURNAL_DEV_OK);
				}
			}
		} else fatMount("usb", usb, 0, 2, 128);
		
		isInserted[DEVICE_USB] = true;
	}
	
	if (isInserted[DEVICE_CARDA]) {
		if (!carda->isInserted()) {
			fatUnmount("carda:");
			carda->shutdown();
			isInserted[DEVICE_CARDA] = false;
		}
	} else if (carda->startup() && carda->isInserted()) {
		fatMount("carda", carda, 0, 4, 64);
		isInserted[DEVICE_CARDA] = true;
	}
	
	if (isInserted[DEVICE_CARDB]) {
		if (!cardb->isInserted()) {
			fatUnmount("cardb:");
			cardb->shutdown();
			isInserted[DEVICE_CARDB] = false;
		}
	} else if (cardb->startup() && cardb->isInserted()) {
		fatMount("cardb", cardb, 0, 4, 64);
		isInserted[DEVICE_CARDB] = true;
	}
}

static void *mountloop(void *arg)
{
	usleep(200 * TB_MSPERSEC);
	
	while (!exit_automount_thread) {
		if (!LWP_MutexTryLock(dvd_mutex)) {
			LWP_MutexLock(dvd_mutex);
			
			if (isInserted[DEVICE_DVD]) {
				if (!dvd->isInserted()) {
					FST_Unmount();
					GCFST_Unmount();
					ISO9660_Unmount();
					isInserted[DEVICE_DVD] = false;
					dvd_lasttick = 0;
				} else if (dvd_lasttick > 0) {
					if (diff_sec(dvd_lasttick, gettime()) > 60) {
						DI_StopMotor();
						dvd_lasttick = 0;
					}
				}
			} else if (dvd->isInserted())
				isInserted[DEVICE_DVD] = true;
			
			LWP_MutexUnlock(dvd_mutex);
		}
		
		mountproc();
		if (exit_automount_thread) break;
		usleep(2 * TB_USPERSEC);
	}
	
	return NULL;
}


static char *default_args[] = {
	"sd:/apps/mplayer-ce/mplayer.dol",
	"-bgvideo", NULL,
	"-idle", NULL,
	"-vo","gekko","-ao","aesnd",
	"-menu","-menu-startup",
	"-quiet"
}; 

static void reset_cb (void) {
	reset_pressed = true;
}

static void power_cb (void) {
	power_pressed = true;
}

#include <sys/time.h>
#include <sys/timeb.h>


void gekko_abort(void) {
	//printf("abort() called\n");
	plat_deinit(-1);
	exit(-1);
}

/******************************************/
/*           NETWORK FUNCTIONS            */
/******************************************/
#define NET_STACKSIZE 8*1024
static u8 net_Stack[NET_STACKSIZE] ATTRIBUTE_ALIGN (32);

lwp_t netthread=LWP_THREAD_NULL;

typedef struct
{
	char* ip;
	char* share;
	char* user;
	char* pass;
	bool init;
} t_smb_conf;

typedef struct
{
	char* ip;
	char* share;
	char* user;
	char* pass;
	int passive;
	bool init;
} t_ftp_conf;
static t_smb_conf smb_conf[5];
static t_ftp_conf ftp_conf[5];

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
	int i;
	if(network_initied) return 1;
	
	for(i=0;i<5;i++)
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
	          network_initied=1;
			  if(dbg_network) printf("Network initialized. IP: %s\n",myIP);
			  usleep(1000);
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

static bool mount_smb(int number)
{	
	char device[10];

	sprintf(device,"smb%d",number+1);

	smb_conf[number].init=true;

	if(smb_conf[number].ip==NULL || smb_conf[number].share==NULL)
	{
		if(dbg_network) printf("SMB %s not filled\n",device);
		usleep(2000); // sync problem on libogc threads
		return false;
	}

	if(smb_conf[number].user==NULL) smb_conf[number].user=strdup("");
	if(smb_conf[number].pass==NULL) smb_conf[number].pass=strdup("");
	
	if(dbg_network)
	{
		u64 t1,t2;
		t1=ticks_to_millisecs(gettime());

		printf("Mounting SMB : '%s' ip:%s  share:'%s'\n",device,smb_conf[number].ip,smb_conf[number].share);
		if(!smbInitDevice(device,smb_conf[number].user,smb_conf[number].pass,smb_conf[number].share,smb_conf[number].ip))
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
	  return smbInitDevice(device,smb_conf[number].user,smb_conf[number].pass,smb_conf[number].share,smb_conf[number].ip);
 
}

bool mount_ftp(int number)
{	
	char device[10];

	sprintf(device,"ftp%i",number+1);

	ftp_conf[number].init=true;

	if(ftp_conf[number].ip==NULL || ftp_conf[number].share==NULL)
	{
		if(dbg_network) printf("FTP %s not filled\n",device);
		usleep(2000);  // sync problem on libogc threads
		return false;
	}

	if(ftp_conf[number].user==NULL) ftp_conf[number].user=strdup("anonymous");
	if(ftp_conf[number].pass==NULL) ftp_conf[number].pass=strdup("anonymous");

	
	if(dbg_network)
	{
		printf("Mounting FTP : '%s' host:%s  share:'%s'\n",device,ftp_conf[number].ip,ftp_conf[number].share);
		u64 t1,t2;
		t1=GetTimerMS();
		if(!ftpInitDevice(device,ftp_conf[number].user,ftp_conf[number].pass,ftp_conf[number].share,ftp_conf[number].ip,21,ftp_conf[number].passive>0))
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
	  return ftpInitDevice(device,ftp_conf[number].user,ftp_conf[number].pass,ftp_conf[number].share,ftp_conf[number].ip,21,ftp_conf[number].passive>0);
}

void read_net_config()
{
	char file[100];
	int i;
	m_config_t *conf;
	m_option_t smb_opts[] =
	{
	    {   "ip1", &smb_conf[0].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share1", &smb_conf[0].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user1", &smb_conf[0].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass1", &smb_conf[0].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "ip2", &smb_conf[1].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share2", &smb_conf[1].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user2", &smb_conf[1].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass2", &smb_conf[1].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "ip3", &smb_conf[2].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share3", &smb_conf[2].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user3", &smb_conf[2].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass3", &smb_conf[2].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "ip4", &smb_conf[3].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share4", &smb_conf[3].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user4", &smb_conf[3].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass4", &smb_conf[3].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "ip5", &smb_conf[4].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share5", &smb_conf[4].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user5", &smb_conf[4].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass5", &smb_conf[4].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};
	m_option_t ftp_opts[] =
	{
	    {   "ip1", &ftp_conf[0].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share1", &ftp_conf[0].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user1", &ftp_conf[0].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass1", &ftp_conf[0].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "passive1", &ftp_conf[0].passive, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "ip2", &ftp_conf[1].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share2", &ftp_conf[1].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user2", &ftp_conf[1].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass2", &ftp_conf[1].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "passive2", &ftp_conf[1].passive, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "ip3", &ftp_conf[2].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share3", &ftp_conf[2].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user3", &ftp_conf[2].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass3", &ftp_conf[2].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "passive3", &ftp_conf[2].passive, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "ip4", &ftp_conf[3].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share4", &ftp_conf[3].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user4", &ftp_conf[3].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass4", &ftp_conf[3].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "passive4", &ftp_conf[3].passive, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "ip5", &ftp_conf[4].ip, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "share5", &ftp_conf[4].share, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "user5", &ftp_conf[4].user, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "pass5", &ftp_conf[4].pass, CONF_TYPE_STRING, 0, 0, 0, NULL },
	    {   "passive5", &ftp_conf[4].passive, CONF_TYPE_FLAG, 0, 0, 1, NULL},

	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};

	for(i=0;i<5;i++)
	{
		smb_conf[i].ip=NULL;
		smb_conf[i].init=false;
	}
	for(i=0;i<5;i++)
	{
		ftp_conf[i].ip=NULL;
		ftp_conf[i].init=false;
	}

	/* read configuration */

	printf_debug("reading samba config");
	sprintf(file,"%s/smb.conf",MPLAYER_DATADIR);
	conf = m_config_new();
	m_config_register_options(conf, smb_opts);
	m_config_parse_config_file(conf, file);
	m_config_free(conf);

	printf_debug("reading ftp config");
	sprintf(file,"%s/ftp.conf",MPLAYER_DATADIR);
	conf = m_config_new();
	m_config_register_options(conf, ftp_opts);
	m_config_parse_config_file(conf, file);
	m_config_free(conf);

}

static void * networkthreadfunc (void *arg)
{
	while(1)
	{
		wait_for_network_initialisation();
		LWP_SuspendThread(netthread);
		net_deinit();
	}
    return NULL;
}

/******************************************/
/*        END NETWORK FUNCTIONS           */
/******************************************/

void DVDGekkoTick(bool silent)
{
	if (!dvd_lasttick) {
		LWP_MutexLock(dvd_mutex);
		
		if (dvd->isInserted()) {
			if (!silent) {
				set_osd_msg(OSD_MSG_TEXT, 1, 5000, "Mounting DVD, please wait");
				force_osd();
			}
			
			if (dvd->startup()) {
				dvd_lasttick = gettime();
				LWP_MutexUnlock(dvd_mutex);
				return;
			}
		}
		
		if (!silent) {
			set_osd_msg(OSD_MSG_TEXT, 1, 2000, "Error mounting DVD");
			force_osd();
		}
		
		LWP_MutexUnlock(dvd_mutex);
	} else dvd_lasttick = gettime();
}

bool DVDGekkoMount()
{
	LWP_MutexLock(dvd_mutex);
	
	if (dvd->isInserted()) {
		if (!dvd_lasttick) {
			set_osd_msg(OSD_MSG_TEXT, 1, 5000, "Mounting DVD, please wait");
			force_osd();
			
			if (!dvd->startup()) {
				LWP_MutexUnlock(dvd_mutex);
				return false;
			} else dvd_lasttick = gettime();
		}
		
		if (ISO9660_Mount() || FST_Mount() || GCFST_Mount()) {
			LWP_MutexUnlock(dvd_mutex);
			return true;
		}
	}
	
	LWP_MutexUnlock(dvd_mutex);
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
  if (!devops) 
  {
  	free(buf);
  	return false;
  }
  for(i=0;buf[i]!='\0' && buf[i]!=':';i++);  
  if (!devops || strncasecmp(buf,devops->name,i))
  {
  	free(buf);
  	return false;
  }
  free(buf);
  return true;
}




static int LoadParams()
{
	char cad[100];
	int ret;
	m_config_t *comp_conf;
	m_option_t comp_opts[] =
	{
	    {   "restore_points", &enable_restore_points, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "watchdog", &enable_watchdog, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   "debug_network", &dbg_network, CONF_TYPE_FLAG, 0, 0, 1, NULL},
		{	"video_mode", &video_mode, CONF_TYPE_INT, CONF_RANGE, 0, 4, NULL},
		{	"overscan", &overscan, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	    {   NULL, NULL, 0, 0, 0, 0, NULL }
	};		
	
	/* read configuration */
	comp_conf = m_config_new();
	m_config_register_options(comp_conf, comp_opts);
	
	sprintf(cad,"%s/mplayer.conf",MPLAYER_DATADIR);
	ret = m_config_parse_config_file(comp_conf, cad); 
	m_config_free(comp_conf);
	return ret;
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
	if(f==NULL)
	{
		printf_debug("mplayer.conf not found in: %s\n",path);
		return false;
	}
	fclose(f);

	sprintf(MPLAYER_DATADIR,"%s",path);
	sprintf(MPLAYER_CONFDIR,"%s",path);
	sprintf(MPLAYER_LIBDIR,"%s",path);
	
	printf_debug("mplayer.conf found in: %s\n",path);
	return true;
}


static bool DetectValidPath()
{
	if (isInserted[DEVICE_SD] && DeviceMounted("sd")) {
		if (CheckPath("sd:/apps/mplayer-ce")) return true;
		if (CheckPath("sd:/apps/mplayer_ce")) return true;
		if (CheckPath("sd:/mplayer")) return true;
	}
	
	if (isInserted[DEVICE_USB] && DeviceMounted("usb")) {
		if (CheckPath("usb:/apps/mplayer-ce")) return true;
		if (CheckPath("usb:/apps/mplayer_ce")) return true;
		if (CheckPath("usb:/mplayer")) return true;
	}
	
	if (isInserted[DEVICE_CARDA] && DeviceMounted("carda")) {
		if (CheckPath("carda:/apps/mplayer-ce")) return true;
		if (CheckPath("carda:/mplayer")) return true;
	}
	
	if (isInserted[DEVICE_CARDB] && DeviceMounted("cardb")) {
		if (CheckPath("cardb:/apps/mplayer-ce")) return true;
		if (CheckPath("cardb:/mplayer")) return true;
	}
	
	return false;	
}

extern u32 __di_check_ahbprot(void);

void plat_init (int *argc, char **argv[])
{
	if (FindIOS(202)) {
		IOS_ReloadIOS(202);
	} else {
		if ((IOS_GetVersion() != 58) && !__di_check_ahbprot()) {
			if (FindIOS(58))
				IOS_ReloadIOS(58);
			else DI_LoadDVDX(true);
		}
	}
	
	DI_Init();
	bool ehci = false;
	
	if (IOS_GetVersion() == 202)
		if (mload_init()) ehci = load_ehci_module();
	
	USB2Enable(ehci);
	
	mpviSetup(0, true);
	log_console_init(vmode, 0);
	mountproc();
	
	if (!DetectValidPath()) {
		printf("\nSD/USB access failed\n");
		printf("Please check that you have installed MPlayer CE in the right folder\n");
		printf("Valid folders:\n");
		printf(" sd:/apps/mplayer-ce\n sd:/mplayer\n usb:/apps/mplayer-ce\n usb:/mplayer\n");
				
		VIDEO_WaitVSync();
		sleep(10);
		mpviClear();
		if (!hbc_stub()) SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
		exit(0);
	}
	
	SYS_SetPowerCallback(power_cb);
	SYS_SetResetCallback(reset_cb);
	
	LoadParams();
	read_net_config();
	load_screen_params();
	
	if(dbg_network)
	{
		printf("\nDebugging Network\n");
		if(wait_for_network_initialisation()) 
		{
			int i;
			for(i=0;i<5;i++) mount_smb(i);
	        for(i=0;i<5;i++) mount_ftp(i);
		}
		network_initied=1;
		printf("Pause for reading (10 seconds)...");
		VIDEO_WaitVSync();
		sleep(10);
	}
	else LWP_CreateThread(&netthread, networkthreadfunc, NULL, net_Stack, NET_STACKSIZE, 64); // network initialization

	chdir(MPLAYER_DATADIR);
	setenv("HOME", MPLAYER_DATADIR, 1);
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
	
	if(enable_watchdog)
		LWP_MutexInit(&watchdogmutex, false);
	
	LWP_MutexInit(&dvd_mutex, false);
	
	LWP_CreateThread(&watchdogthread, watchdogthreadfunc, NULL, watchdog_Stack, WATCHDOG_STACKSIZE, 64);
	LWP_CreateThread(&mountthread, mountloop, NULL, mount_Stack, MOUNT_STACKSIZE, 40); // auto mount fs (usb, dvd)
	
	VIDEO_WaitVSync();
	
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	
	log_console_enable_video(false);
	printf("\n\n");
	
	if ((video_mode > 0) || !overscan)
	{
		log_console_deinit();
		mpviClear();
		mpviSetup(video_mode, overscan);
		log_console_init(vmode, 0);
		log_console_enable_video(false);
	}
}

void plat_deinit (int rc) 
{
	log_console_deinit();
	mpviClear();
	
	exit_automount_thread=true;
	LWP_JoinThread(mountthread,NULL);
	exit_watchdog_thread=true;
	LWP_JoinThread(watchdogthread,NULL);
	if(watchdogmutex!=LWP_MUTEX_NULL)LWP_MutexDestroy(watchdogmutex);
	save_screen_params();

	if (power_pressed) {
		//printf("shutting down\n");
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
    if (!hbc_stub())
		SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
}

extern float m_screenleft_shift, m_screenright_shift;
extern float m_screentop_shift, m_screenbottom_shift;
extern bool nunchuk_update;

void load_screen_params()
{
	int i;
	FILE *f;
	char aux[120];
	float value;
	char param[50];
	sprintf(aux,"%s/%s",MPLAYER_DATADIR,"screen.conf");
	f=fopen(aux,"r");
	if(f==NULL) return;
	setvbuf(f,NULL,_IONBF,0);
	//printf("loading : %s------------------\n",aux);
	for(i=0; !feof(f) ;i++)
	{
		fscanf(f,"%[^\t]%f\n",param,&value);
		//printf("param: %s   value: %2f\n",param,value);
		if(strcmp(param,"screenleft_shift")==0) m_screenleft_shift=value;
		else if(strcmp(param,"screenright_shift")==0) m_screenright_shift=value;
		else if(strcmp(param,"screentop_shift")==0) m_screentop_shift=value;
		else if(strcmp(param,"screenbottom_shift")==0) m_screenbottom_shift=value;
	}
	fclose(f);

}

void save_screen_params()
{
	FILE *f;
	char aux[1024];
	char buff[1024];
	if(nunchuk_update==false) return;
	buff[0]='\0';
	sprintf(aux,"%s/%s",MPLAYER_DATADIR,"screen.conf");
	f=fopen(aux,"wb+");
	if(f==NULL)
	{
		return;
	}
	sprintf(aux,"%s\t%2f\n","screenleft_shift",m_screenleft_shift);
	strcat(buff,aux);
	sprintf(aux,"%s\t%2f\n","screenright_shift",m_screenright_shift);
	strcat(buff,aux);
	sprintf(aux,"%s\t%2f\n","screentop_shift",m_screentop_shift);
	strcat(buff,aux);
	sprintf(aux,"%s\t%2f\n","screenbottom_shift",m_screenbottom_shift);
	strcat(buff,aux);

	fwrite( buff, sizeof(char), strlen(buff), f );
	fclose(f);
}

bool smbConnect(char *device)
{
	int number;
	if(network_initied==0) return false;
	number=device[3]-'0';
	number=number-1;
	if(!smb_conf[number].init) return mount_smb(number);
	return smbCheckConnection(device);
}

bool ftpConnect(char *device)
{
	int number;
	if(network_initied==0) return false;
	number=device[3]-'0';
	number=number-1;
	if(!ftp_conf[number].init) return mount_ftp(number);
	return CheckFTPConnection(device);
}
