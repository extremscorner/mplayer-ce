/*-------------------------------------------------------------

usb2storage.c -- USB mass storage support, inside starlet
Copyright (C) 2008 Kwiirk
Improved for homebrew by rodries

If this driver is linked before libogc, this will replace the original 
usbstorage driver by svpe from libogc

CIOS_usb2 must be loaded!

This software is provided 'as-is', without any express or implied
warranty.	In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/
#if defined(HW_RVL)
#include <gccore.h>

#include <ogc/lwp_heap.h>
#include <malloc.h>
#include <ogc/disc_io.h>
#include <ogc/usbstorage.h>
#include <ogc/mutex.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> 


//#define DEBUG_USB2

#ifdef DEBUG_USB2
#define debug_printf(fmt, args...) \
	fprintf(stderr, "%s:%d:" fmt, __FUNCTION__, __LINE__, ##args)
#else
#define debug_printf(fmt, args...)
#endif // DEBUG_USB2


#define UMS_BASE (('U'<<24)|('M'<<16)|('S'<<8))
#define USB_IOCTL_UMS_INIT						(UMS_BASE+0x1)
#define USB_IOCTL_UMS_GET_CAPACITY				(UMS_BASE+0x2)
#define USB_IOCTL_UMS_READ_SECTORS				(UMS_BASE+0x3)
#define USB_IOCTL_UMS_WRITE_SECTORS		(UMS_BASE+0x4)
#define USB_IOCTL_UMS_READ_STRESS		(UMS_BASE+0x5)
#define USB_IOCTL_UMS_SET_VERBOSE		(UMS_BASE+0x6)
#define USB_IOCTL_UMS_IS_INSERTED		(UMS_BASE+0x7)


#define USB_IOCTL_UMS_UMOUNT			(UMS_BASE+0x10)
#define USB_IOCTL_UMS_START			(UMS_BASE+0x11)
#define USB_IOCTL_UMS_STOP			(UMS_BASE+0x12)
#define USB_IOCTL_UMS_EXIT			(UMS_BASE+0x16)

#define UMS_HEAPSIZE					2*1024
#define UMS_MAXPATH 16

static bool usb2disabled = false;
static bool usb1disabled = false;
static s32 hId = -1;
static s32 fd=-1;
static u32 sector_size;
static s32 usb2=-1;
static mutex_t usb2_mutex = LWP_MUTEX_NULL;
static u8 *fixed_buffer = NULL;
static s32 usb2_init_value=-1;

extern const DISC_INTERFACE __io_usb2storage;


#include <ogc/machine/processor.h>

#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)
#define USB2_BUFFER 128*1024
static heap_cntrl usb2_heap;
static u8 __usb2_heap_created = 0;

static s32 USB2CreateHeap() {
	u32 level;
	void *usb2_heap_ptr;	
	
	_CPU_ISR_Disable(level);

	if(__usb2_heap_created != 0) {
		_CPU_ISR_Restore(level);
		return IPC_OK;
	}
	
	usb2_heap_ptr = (void *)ROUNDDOWN32(((u32)SYS_GetArena2Hi() - (USB2_BUFFER+(4*1024))));
	if((u32)usb2_heap_ptr < (u32)SYS_GetArena2Lo()) {
		_CPU_ISR_Restore(level);
		return IPC_ENOMEM;
	}
	SYS_SetArena2Hi(usb2_heap_ptr);
	__lwp_heap_init(&usb2_heap, usb2_heap_ptr, (USB2_BUFFER+(4*1024)), 32);
	__usb2_heap_created=1;
	_CPU_ISR_Restore(level);
	return IPC_OK;
}

static void* usb2_malloc(u32 size)
{
	return __lwp_heap_allocate(&usb2_heap, size);
}

BOOL usb2_free(void *ptr)
{
	return __lwp_heap_free(&usb2_heap, ptr);
}

static s32 USBStorage_Init(int verbose)
{
	//s32 _fd = -1;
	s32 ret = USB_OK;
	u32 size=0;
	char *devicepath = NULL;
	if(usb2_mutex == LWP_MUTEX_NULL) LWP_MutexInit(&usb2_mutex, false);	
	LWP_MutexLock(usb2_mutex);
	//if(fd!=-1) IOS_Close(fd);
	//fd=-1;

	usb2_init_value=-2;
	if(hId==-1) hId = iosCreateHeap(UMS_HEAPSIZE);
	if(hId<0) 
	{
		LWP_MutexUnlock(usb2_mutex);
		return IPC_ENOMEM;
	} 
	
	if(USB2CreateHeap()!=IPC_OK)return IPC_ENOMEM;
	if(fixed_buffer == NULL) fixed_buffer = usb2_malloc(USB2_BUFFER);
	
	if(fd<0)
	{
		devicepath = iosAlloc(hId,UMS_MAXPATH);
		if(devicepath==NULL)
		{
			LWP_MutexUnlock(usb2_mutex);
			return IPC_ENOMEM;
		}
		
		//snprintf(devicepath,USB_MAXPATH,"/dev/usb/ehc");
		snprintf(devicepath,USB_MAXPATH,"/dev/usb2");
		fd = IOS_Open(devicepath,0);
		
		iosFree(hId,devicepath);
	}
	ret=fd;
	debug_printf("usb2 fd: %d\n",fd);
	usleep(500);
	if(fd>0)
	{
		if(verbose)
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_SET_VERBOSE,":");
		ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_INIT,":");
		usb2_init_value=ret;		
		//printf("usb2 init value: %i\n", ret);
		if(ret<0) 
		{
			debug_printf("usb2 error init\n");
		}
		else
			size = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_GET_CAPACITY,":i",&sector_size);
		debug_printf("usb2 GET_CAPACITY: %d\n",	size);	
		
		if(size==0) ret=-2012;	
		else ret=1;
	} 
	else ret=-1;
	 
	LWP_MutexUnlock(usb2_mutex);
	return ret;
}
/*
static s32 USBStorage_Get_Capacity(u32*_sector_size)
{
	if(fd>0)
	{
		LWP_MutexLock(usb2_mutex);
		s32 ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_GET_CAPACITY,":i",&sector_size);
		if(_sector_size) *_sector_size = sector_size;
		LWP_MutexUnlock(usb2_mutex);
		return ret;
	}
	else return IPC_ENOENT;
}
*/
static inline int is_MEM2_buffer(const void *buffer)
{
	u32 high_addr = ((u32)buffer)>>24;
	return (high_addr == 0x90) ||(high_addr == 0xD0);
}

static s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer)
{
	s32 ret=1; 
	u32 sectors=0;
	uint8_t *dest = buffer;

	if(fd<1) return IPC_ENOENT;

	LWP_MutexLock(usb2_mutex);
	
	while(numSectors>0)
	{
		if(numSectors*sector_size > USB2_BUFFER)
			sectors=USB2_BUFFER/sector_size;
		else 
			sectors=numSectors;

		if (!is_MEM2_buffer(dest)) //libfat is not providing us good buffers :-(
		{
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_SECTORS,"ii:d",sector,sectors,fixed_buffer,sector_size*sectors);
			memcpy(dest,fixed_buffer,sector_size*sectors);
		}
		else ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_SECTORS,"ii:d",sector,sectors,dest,sector_size*sectors);

		dest+=sector_size*sectors;
		if(ret<1)break;

		sector+=sectors;
		numSectors-=sectors;
	}
	LWP_MutexUnlock(usb2_mutex);
	return ret;
}

static s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer)
{
	s32 ret=1;
	u32 sectors=0;
	uint8_t *dest = (uint8_t *)buffer;
	if(fd<1) return IPC_ENOENT;

	LWP_MutexLock(usb2_mutex);
	while(numSectors>0 && ret>0)
	{
		if(numSectors*sector_size > USB2_BUFFER)
			sectors=USB2_BUFFER/sector_size;
		else 
			sectors=numSectors;

		numSectors-=sectors;
		
	
		if (!is_MEM2_buffer(dest)) // libfat is not providing us good buffers :-(
		{
			memcpy(fixed_buffer,dest,sector_size*sectors);
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_WRITE_SECTORS,"ii:d",sector,sectors,fixed_buffer,sector_size*sectors);
		}
		else ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_WRITE_SECTORS,"ii:d",sector,sectors,dest,sector_size*sectors);
		dest+=sector_size*sectors;
		sector+=sectors;
	}
	LWP_MutexUnlock(usb2_mutex);
	return ret;	
}


bool USB2Available()
{
	int ret;
	if(usb2disabled) return false;
	ret=USBStorage_Init(0);
	if(ret<0) return false;
	usb2=1;
	return true;
}

static bool __usb2storage_Startup(void)
{
	debug_printf("__usb2storage_Startup\n");
	if(usb2disabled)
	{
		//__io_usbstorage = __io_usb1storage;
		//return __io_usbstorage.startup();		
	}
	usb2 = USBStorage_Init(0);
				
	if(usb2 < 0 && !usb1disabled) 
	{
		//__io_usbstorage = __io_usb1storage;
		//return __io_usbstorage.startup();
	}
	__io_usbstorage = __io_usb2storage;
	return usb2>=0;
}

static bool __usb2storage_IsInserted(void)
{
	if(usb2==-1) 
	{
		bool ret;
		ret=__usb2storage_Startup();
		//if(usb2==-1) return false;
		return ret;
	}
	if(fd>0)
	{
		LWP_MutexLock(usb2_mutex);
		int ret;
		ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_IS_INSERTED,":");
		debug_printf("isinserted usb2 ret: %d\n",ret);
		
		if(ret>0) 
		{
			LWP_MutexUnlock(usb2_mutex);
			return true;
		}
		else
		{
			//ret=__io_usb1storage.isInserted();
			
			if(ret>0)
			{
				//__io_usbstorage = __io_usb1storage;
				LWP_MutexUnlock(usb2_mutex);
				return true;
			}
		}
		LWP_MutexUnlock(usb2_mutex);
	}
	return false;
}

static bool __usb2storage_ClearStatus(void)
{
	return true;
}

static bool __usb2storage_Shutdown(void)
{
	LWP_MutexLock(usb2_mutex);
	usb2=-1;
	LWP_MutexUnlock(usb2_mutex);
	return true;
}

void DisableUSB2(bool state)
{
	usb2disabled = state;
}

void DisableUSB1(bool state)
{
	usb1disabled = state;
}

s32 GetInitValue()
{
	return usb2_init_value;
}

s32 USB2Unmount()
{
	return IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_UMOUNT,":");
}
s32 USB2Start()
{
	return IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_START,":");
}
s32 USB2Stop()
{
	return IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_STOP,":");
}

void USB2Close()
{
	if(fd>0)IOS_Close(fd);
	fd=-1;
}

int USB2ReadSector(u32 sector)
{
	void *b;
	s32 ret;
	b = malloc(1024);	
	ret = USBStorage_Read_Sectors(sector, 1, b);
	free(b);
	return ret;
}

const DISC_INTERFACE __io_usb2storage = 
{
	DEVICE_TYPE_WII_USB,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB,
	(FN_MEDIUM_STARTUP)&__usb2storage_Startup,
	(FN_MEDIUM_ISINSERTED)&__usb2storage_IsInserted,
	(FN_MEDIUM_READSECTORS)&USBStorage_Read_Sectors,
	(FN_MEDIUM_WRITESECTORS)&USBStorage_Write_Sectors,
	(FN_MEDIUM_CLEARSTATUS)&__usb2storage_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&__usb2storage_Shutdown
};

#endif /* HW_RVL */
