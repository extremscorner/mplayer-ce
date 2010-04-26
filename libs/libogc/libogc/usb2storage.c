/*-------------------------------------------------------------

usb2storage.c -- USB mass storage support, inside starlet
Copyright (C) 2008 Kwiirk

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

#define UMS_BASE (('U'<<24)|('M'<<16)|('S'<<8))
#define USB_IOCTL_UMS_INIT						(UMS_BASE+0x1)
#define USB_IOCTL_UMS_GET_CAPACITY				(UMS_BASE+0x2)
#define USB_IOCTL_UMS_READ_SECTORS				(UMS_BASE+0x3)
#define USB_IOCTL_UMS_WRITE_SECTORS		(UMS_BASE+0x4)
#define USB_IOCTL_UMS_READ_STRESS		(UMS_BASE+0x5)
#define USB_IOCTL_UMS_SET_VERBOSE		(UMS_BASE+0x6)

#define UMS_HEAPSIZE					65*1024
#define UMS_MAXPATH 16


static s32 hId = -1;
static s32 fd=-1;
static u32 sector_size;
static s8 usb2=-1;
static mutex_t usb2_mutex = LWP_MUTEX_NULL;
static u8 *fixed_buffer = NULL;
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

static s32 USBStorage_Init(int verbose)
{
	s32 _fd = -1;
	s32 ret = USB_OK;
	char *devicepath = NULL;
	
	if(usb2_mutex == LWP_MUTEX_NULL) LWP_MutexInit(&usb2_mutex, false);	
		
	LWP_MutexLock(usb2_mutex);
	if(fd!=-1) IOS_Close(fd);


	if(hId==-1) hId = iosCreateHeap(UMS_HEAPSIZE);
	if(hId<0) 
	{
		LWP_MutexUnlock(usb2_mutex);
		return IPC_ENOMEM;
	} 
	if(fixed_buffer == NULL) fixed_buffer = iosAlloc(hId,64*1024);
	devicepath = iosAlloc(hId,UMS_MAXPATH);
	if(devicepath==NULL)
	{
		LWP_MutexUnlock(usb2_mutex);
		return IPC_ENOMEM;
	}
	
	snprintf(devicepath,USB_MAXPATH,"/dev/usb/ehc");

	_fd = IOS_Open(devicepath,0);
	if(_fd<0) ret = _fd;
	else fd = _fd;

	iosFree(hId,devicepath);

	
	if(fd>0)
	{
		if(verbose)
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_SET_VERBOSE,":");
		IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_INIT,":");
		ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_GET_CAPACITY,":i",&sector_size);
		if(ret==0)
		{
			IOS_Close(fd);
			fd=-1;
			ret=-1;
		}
	} 
	LWP_MutexUnlock(usb2_mutex);
	return ret;
}

static inline int is_MEM2_buffer(const void *buffer)
{
	u32 high_addr = ((u32)buffer)>>24;
	return (high_addr == 0x90) ||(high_addr == 0xD0);
}

static s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer)
{
//todo: max size to read/write is 62kb, I have to do a loop if sector_size*numSectors>62kb
	if(fd>0)
	{
		s32 ret;
		LWP_MutexLock(usb2_mutex);
		
		if (!is_MEM2_buffer(buffer)) //libfat is not providing us good buffers :-(
		{
		/*
		
			u8 *_buffer = iosAlloc(hId,sector_size*numSectors);
			if(!_buffer)
			{
				LWP_MutexUnlock(usb2_mutex);
				return IPC_ENOMEM;
			}
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_SECTORS,"ii:d",sector,numSectors,_buffer,sector_size*numSectors);
			memcpy(buffer,_buffer,sector_size*numSectors);
			iosFree(hId,_buffer);
			*/
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_SECTORS,"ii:d",sector,numSectors,fixed_buffer,sector_size*numSectors);
			memcpy(buffer,fixed_buffer,sector_size*numSectors);
		}
		else ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_SECTORS,"ii:d",sector,numSectors,buffer,sector_size*numSectors);
		LWP_MutexUnlock(usb2_mutex);
		
		return ret;
	}
	else return IPC_ENOENT;
}

static s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer)
{
	s32 ret;
	if(fd>0)
	{
		LWP_MutexLock(usb2_mutex);
		if (!is_MEM2_buffer(buffer)) // libfat is not providing us good buffers :-(
		{/*
			u8 *_buffer = iosAlloc(hId,sector_size*numSectors);
			if(!_buffer)
			{
				LWP_MutexUnlock(usb2_mutex);
				return IPC_ENOMEM;
			}
			memcpy(_buffer,buffer,sector_size*numSectors);
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_WRITE_SECTORS,"ii:d",sector,numSectors,_buffer,sector_size*numSectors);
			iosFree(hId,_buffer);
			*/
			memcpy(fixed_buffer,buffer,sector_size*numSectors);
			ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_WRITE_SECTORS,"ii:d",sector,numSectors,fixed_buffer,sector_size*numSectors);
		}
		else ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_WRITE_SECTORS,"ii:d",sector,numSectors,buffer,sector_size*numSectors);
		LWP_MutexUnlock(usb2_mutex);
		return ret;
	}
	else return IPC_ENOENT;
}


static bool USB2Available()
{
	s32 _fd;
	s32 _hId = -1;
	char *devicepath = NULL;
	
	if(usb2_mutex == LWP_MUTEX_NULL) LWP_MutexInit(&usb2_mutex, false);	
	
	_hId = iosCreateHeap(1024);
	if(_hId<0) return false; 

	devicepath = iosAlloc(_hId,UMS_MAXPATH);
	if(devicepath==NULL)return false;

	LWP_MutexLock(usb2_mutex);
	snprintf(devicepath,USB_MAXPATH,"/dev/usb/ehc");

	_fd = IOS_Open(devicepath,0);
	IOS_Close(_fd);
	iosFree(_hId,devicepath);
	iosDestroyHeap(_hId);
	LWP_MutexUnlock(usb2_mutex);	
	if(_fd>0) return true;
	return false;
}

static bool __usb2storage_Startup(void)
{
	//The first time I try to access usb I detect usb1 or usb2
	if(usb2 != -1) return 1;
	if(usb2 == -1)
	{
		usb2 = USB2Available();
	}	
	if(usb2 == 0) 
	{
		__io_usbstorage = __io_usb1storage;
		return __io_usbstorage.startup();
	}
	
	return USBStorage_Init(0);
}

static bool __usb2storage_IsInserted(void)
{
	if(usb2==-1) 
	{
		__usb2storage_Startup();
		if(usb2 == 0) return __io_usbstorage.isInserted();
	}
	if(fd>0)
	{
		LWP_MutexLock(usb2_mutex);
		u8 *buf = iosAlloc(hId,sector_size);
		int ret;
		ret = USBStorage_Read_Sectors(0, 1, buf);
		iosFree(hId,buf);
		if(ret<1)
		{
			IOS_Close(fd);
			fd=-1;
		}
		LWP_MutexUnlock(usb2_mutex);
		if(ret>0) return true;
		return USBStorage_Init(0)>0;
	}
	else
	{
		return USBStorage_Init(0)>0;
	}
}

static bool __usb2storage_ClearStatus(void)
{
	return true;
}

static bool __usb2storage_Shutdown(void)
{
	LWP_MutexLock(usb2_mutex);
	if(fd>0) IOS_Close(fd);
	fd=-1;
	LWP_MutexUnlock(usb2_mutex);
	return true;
}

DISC_INTERFACE __io_usbstorage = 
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
