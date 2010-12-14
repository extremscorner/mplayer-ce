/*

Elf/Dol FOrwarder -- loads an elf or dol specified in the code.

by SpaceJump (2008)
modified by MrGreg 12/02/2010

Thanks to svpe, the creator of Front SD Loader and TCPLoad for giving me permission for using some of his
functions.

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from
the use of this software.

*/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <sys/stat.h>
#include <fat.h>
#include <string.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

//#include "elf_abi.h"
#include <ogc/machine/processor.h>

//USB2 includes
#include "mload.h"
#include "../build/ehcmodule_elf.h"

u32 load_dol_image (void *dolstart, struct __argv *argv);
void __exception_closeall();

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
typedef void (*entrypoint) (void);
const DISC_INTERFACE* sd = &__io_wiisd;
const DISC_INTERFACE* usb = &__io_usbstorage;
bool USB2Enable();

void restart()
{
printf("Rebooting Wii...\n");
sleep(5);
SYS_ResetSystem(SYS_RESTART,0,0);
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
		if((titles[n] &  0xFFFFFFFF)==ios) 
		{
			free(titles); 
			return true;
		}
	}
	
    free(titles); 
	return false;
}

void load_USB2_driver()
{
	int  ret = -1;
	bool usb2 = false;
	u32  iosversion;
	
	//Load usb2 if possible
	USB2Enable(false);
	if (IOS_GetVersion() == 58)
		usb2 = true;
	else if (FindIOS(58))
	{
		IOS_ReloadIOS(58);
		usb2 = true;
	}	
	else if (FindIOS(202))
	{
		IOS_ReloadIOS(202);
		ret=mload_init();
	
		if(ret)
		{
			data_elf my_data_elf;
			mload_elf((void *) ehcmodule_elf, &my_data_elf);
			mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio);
			USB2Enable(true);
			usb2 = true;
//			//printf("Running... at 0x%x\n", (u32) my_data_elf.start);
		}
	}	
	else if (FindIOS(61))
			IOS_ReloadIOS(61);

	if (usb2)
	{
		iosversion = IOS_GetVersion();
		printf("usb2 IOS%u detected!\n",iosversion);
	}	
	else
		printf("usb2 IOS not detected\n");
}

//---------------------------------------------------------------------------------
#define CHUNCK 64*1024
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	
	// Initialise the video system
	VIDEO_Init();
	
	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	
	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);
	
	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);
	
	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	printf("\x1b[2;0H");
	
	//------------------------------------------------------------------------------------------------------

	FILE* inputFile=NULL;
	long  size=0;
	char* device;

	// Try to load usb2 if available
	printf("Detecting usb2 ios...\n");
	load_USB2_driver();
	
	// Try to find app on sd
	if(sd->startup() && sd->isInserted()) 
	{
		printf("Mounting sd FAT partition\n");
		if(fatMount("sd",sd,0,2,128))
		{
			device = "sd";
			inputFile = fopen("sd:/apps/mplayer-ce/boot.dol", "rb");
			if(inputFile == NULL) {
				inputFile = fopen("sd:/apps/mplayer_ce/boot.dol", "rb");
				if(inputFile == NULL) {
					inputFile = fopen("sd:/mplayer/boot.dol", "rb");
				}
			}
		}
		else
			printf("Failed to mount FAT partition on sd!\n");
	}
	// If the app was not found on sd, try and find it on usb
	u64 start = gettime();
    while(inputFile == NULL && diff_sec(start, gettime()) < 5) // 5 sec
    {
		if(usb->startup() && usb->isInserted())
		{
			printf("Mounting usb FAT partition\n");
			if(fatMount("usb",usb,0,2,128))
			{
				device = "usb";
				inputFile = fopen("usb:/apps/mplayer-ce/boot.dol", "rb");
				if(inputFile == NULL) {
					inputFile = fopen("usb:/apps/mplayer_ce/boot.dol", "rb");
					if(inputFile == NULL) {
						inputFile = fopen("usb:/mplayer/boot.dol", "rb");
					}
				}	
			}	
			else
				printf("Failed to mount FAT partition on usb!\n");
		}
		else
			usleep(250000); // 1/4 sec;
    }
		
	if(inputFile != NULL)
		printf("Found boot.dol on %s!\n",device);
	else
	{
		printf("boot.dol does not exist in the /apps/mplayer-ce or /mplayer folder on sd/usb !\n");
		restart();
	}		

	//create a buffer for the dol content
	void* myBuffer=NULL;
	
	//read dol from given path:
	setvbuf(inputFile,NULL,_IONBF,0);
	fseek(inputFile, 0, SEEK_END);
	size = ftell(inputFile);
	fseek(inputFile, 0, SEEK_SET); 
	printf("File size: %ld bytes\n",size);
	myBuffer = malloc(size);
	if(myBuffer == NULL)
	{
		printf("Error allocating memory\n");
		restart();	
	} 
	else
	{
		printf("Memory allocated\n");
		printf("Reading file, please wait...\n");
		printf("[");
		printf("\x1b[s");  
		printf("                                                 ]");
		printf("\x1b[u");
	}
	u32 bytes_read,readed=0;
	int aux,old=0;
	while(1)	
	{
		bytes_read = fread(myBuffer+readed, 1, CHUNCK, inputFile); 
		if (bytes_read < CHUNCK)
		{
			s32 result = -!feof(inputFile);
			readed+=bytes_read;
			if (result < 0 || readed!=size) 
			{
				printf("fread error: [%i] %s\n", ferror(inputFile), strerror(ferror(inputFile)));
				fclose(inputFile);
				restart();	
			}
		break;
		}
     
		readed+=bytes_read;
		aux=(readed*50)/size;
		if(old!=aux)
		{
			old=aux;
			printf(".");
			VIDEO_WaitVSync();
		}
	}
	
    printf("\nFile read successfully\n"); 
  
	// Cleanup
	fclose(inputFile);
	fatUnmount("sd");
    fatUnmount("usb");
	USB_Deinitialize();
	
	//Load the dol
	printf("Loading dol...\n");
	VIDEO_WaitVSync();

	u32 exeEntryPointAddress = 0;
    entrypoint exeEntryPoint;
	
	struct __argv args[10];
	exeEntryPointAddress = load_dol_image(myBuffer,args);		
	printf("dol loaded, executing...\n");

	if (exeEntryPointAddress == 0)
	{
		printf ("Invalid entry point\n");
        restart();
    }
	exeEntryPoint = (entrypoint) exeEntryPointAddress;	
    //printf("Entry point: 0x%X\n", (unsigned int)exeEntryPointAddress);

	//sleep(5);
	// Reinitialize Video
	VIDEO_WaitVSync();	
	//printf("VIDEO_WaitVSync() done\n");

	// Shutdown Subsystems
	__IOS_ShutdownSubsystems ();
	//printf("IOS_ShutdownSubsystems() done\n");

	// Disable CPU ISR
	u32 level;
	_CPU_ISR_Disable (level);
	//printf("_CPU_ISR_Disable() done\n");
	
	// Exception Close All
	__exception_closeall ();
	//printf("__exception_closeall() done. Jumping to ep now...\n");

	// Execute the entrypoint
	exeEntryPoint();
	
	// Restore CPU ISR
	_CPU_ISR_Restore (level);
		
	exit(0);
	return 0;
}
