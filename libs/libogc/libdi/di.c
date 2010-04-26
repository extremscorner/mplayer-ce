#include "di.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ogc/ipc.h>
#include <ogc/ios.h>
#include <ogc/mutex.h>
#include <errno.h>

int di_fd = -1;

int _DI_ReadDVD_ReadID(void* buf, uint32_t len, uint32_t lba);
int _DI_ReadDVD_ReadID_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);

int _DI_ReadDVD_A8(void* buf, uint32_t len, uint32_t lba);
int _DI_ReadDVD_D0(void* buf, uint32_t len, uint32_t lba);

int _DI_ReadDVD_A8_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);
int _DI_ReadDVD_D0_Async(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb);

void _DI_SetCallback(int di_command, ipccallback);
static int _cover_callback(int ret, void* usrdata);

int state = DVD_INIT | DVD_NO_DISC;

static unsigned int bufferMutex = 0;
static uint32_t outbuf[8] __attribute__((aligned(32)));
static uint32_t dic[8] __attribute__((aligned(32)));
static char di_path[] ATTRIBUTE_ALIGN(32) = "/dev/di";

read_func DI_ReadDVDptr = NULL;
read_func_async DI_ReadDVDAsyncptr = NULL;
di_callback di_cb = NULL;


//-----------------------------------------
//------------------ rodries --------------
//-- remove comments after testing
//-----------------------------------------
#include <unistd.h>
#include <malloc.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>

#define DEFAULT_TIME_STOP_MOTOR 60  // 20 secs
#define MIN_TIME_STOP_MOTOR 8  // the min value accepted
static bool motorthreadexit = false;
static lwp_t main_thread;
static bool motor_stopped=true;
static unsigned int LastAccess=0;
static unsigned int TimeStopMotor=DEFAULT_TIME_STOP_MOTOR;

void SetDVDMotorStopSecs(int secs)  //in seconds
{
  if(secs<MIN_TIME_STOP_MOTOR) return;
  LastAccess=ticks_to_secs(gettime());
  TimeStopMotor=secs;
}

unsigned int GetDVDMotorStopSecs()  //in seconds
{
  return TimeStopMotor;
}

static bool DVD_DiscPresent()
{
	uint32_t val;

	DI_GetCoverRegister(&val);	
	if(val&0x2)return true;
	return false;
}

static void * motorthreadfunc (void *ptr)
{
  unsigned int t;
  bool first=true;
  while(1)
  {
    if(motorthreadexit) break;

    if(motor_stopped)
    {
      usleep(10000);
      first=true;
    }  
    else 
    { 
      if(first) // change state from stop to start
      {
        sleep(10); // time to get full spining
        LastAccess=ticks_to_secs(gettime());
        first=false;
      }
      t=ticks_to_secs(gettime());
      if((LastAccess+1+(TimeStopMotor*2)) < LastAccess) LastAccess=t; // prevent overflow (I think is not need, but...)  
      if(t<LastAccess) LastAccess=t; // strange,  perhaps overflow 
      if( (t>LastAccess) && ((t-LastAccess) > TimeStopMotor) && ((t-LastAccess) < (TimeStopMotor*2)))
      {  // we have to stop motor        
        //printf("\n motorthreadfunc: stop motor  t: %u  last: %u \n",t,LastAccess);VIDEO_WaitVSync(); 
        if(DVD_DiscPresent()) // only stop is dvd is present, perhpas 
        {
          DI_StopMotor();            
        }
      }  
      usleep(10000);
    }
    
  }
  LWP_JoinThread(main_thread, NULL);
  return NULL;
}

int DI_StartMotor(){
	if(state == (DVD_INIT | DVD_NO_DISC) ) return 0;
	if(motor_stopped && motorthreadexit==false){ 
		DI_Reset();
		uint32_t val;
		int i=200;    
		do{  
		   usleep(100);
		   DI_GetCoverRegister(&val);
		   i--;
		   if(i==0) break;
		}while(!(val&0x2)); // wait until dvd is ok
		LastAccess=ticks_to_secs(gettime());
		if(i>0) motor_stopped=false; // if you can't start motor then dvd is hang	
		return 1;
	}	
	return 0;
}

void CheckAccess()
{
	DI_StartMotor();
	LastAccess=ticks_to_secs(gettime());  
}

///// Cache
#define CACHE_FREE 0xFFFFFFFF
#define BLOCK_SIZE 0x800
#define CACHEBLOCKS 26
typedef struct
{
	uint32_t block;
	void *ptr;
} cache_page;
static cache_page *cache_read=NULL;

static void CreateDVDCache()
{
  if(cache_read!=NULL) return;
	cache_read=(cache_page *)malloc(sizeof(cache_page));
	if(cache_read==NULL)return;

		cache_read->block=CACHE_FREE;	 
		cache_read->ptr=memalign(32, BLOCK_SIZE * CACHEBLOCKS);
		if(cache_read->ptr==NULL)
		{
			if(cache_read->ptr)free(cache_read->ptr);
			free(cache_read);
			cache_read=NULL;
			return;
		}
		memset(cache_read->ptr, 0, BLOCK_SIZE);	
}
    
static int ReadBlockFromCache(void *buf, uint32_t len, uint32_t block)
{
	int retval;

	if(cache_read==NULL) return DI_ReadDVDptr(buf,len,block);

	if( (block >= cache_read->block) && (block + len < (cache_read->block+CACHEBLOCKS)) )
	{
		memcpy(buf,cache_read->ptr + ((block - cache_read->block) * BLOCK_SIZE), BLOCK_SIZE * len);
		return 0;
	}

  if(len>CACHEBLOCKS) return DI_ReadDVDptr(buf,len,block);
	retval=DI_ReadDVDptr(cache_read->ptr,CACHEBLOCKS,block);
	if(retval)
  {
    cache_read->block=CACHE_FREE;
    return retval;
  }
	
	cache_read->block=block;	 
	memcpy(buf, cache_read->ptr, len*BLOCK_SIZE);	 

	return 0;
}


//-----------------------------------------
//------------ end rodries --------------
//-- remove comments after testing
//-----------------------------------------



/*
Initialize the DI interface, should always be called first!
*/

s32 __DI_LoadStub();

int DI_Init(){
	static int init = 0;

	state = DVD_INIT | DVD_NO_DISC;

	if(!init){
		__DI_LoadStub();	// Marcan's 1337 magics happen here!
		LWP_MutexInit(&bufferMutex, false);
		init = 1;
	}

	if(di_fd < 0)
		di_fd = IOS_Open(di_path, 2);

//------------------ rodries --------------  
  LastAccess=0;
  CreateDVDCache();
//---------------- end rodries ------------  
	return (di_fd >= 0)? di_fd : -1;
}

void DI_Mount(){
	state = DVD_INIT | DVD_NO_DISC;
	_cover_callback(1, NULL);	// Initialize the callback chain.



    int ret;
		void *_buff;
		_buff=memalign(32, BLOCK_SIZE );
		ret=_DI_ReadDVD_D0(_buff, 1, 0);
		if(_buff!=NULL)free(_buff);
		
		if(ret==0)
		{
			state |= DVD_D0;
			DI_ReadDVDptr = _DI_ReadDVD_D0;
			DI_ReadDVDAsyncptr = _DI_ReadDVD_D0_Async;
		}
		else
    {
			state |= DVD_A8;
			DI_ReadDVDptr = _DI_ReadDVD_A8;
			DI_ReadDVDAsyncptr = _DI_ReadDVD_A8_Async;
		}

		
		if(di_cb)
			di_cb(state,0);
		

//------------------ rodries --------------  
  LastAccess=ticks_to_secs(gettime());
  motorthreadexit=true;  // close thread
  usleep(20000); // to be sure is closed
  motorthreadexit=false;
  motor_stopped=false;
  lwp_t motorthread;  
	main_thread = LWP_GetSelf();	
	if(cache_read!=NULL) cache_read->block=CACHE_FREE;  // reset cache	 
	LastAccess=ticks_to_secs(gettime());
	LWP_CreateThread(&motorthread, motorthreadfunc, NULL, NULL, 0, 80);  // create thread  
//---------------- end rodries ------------  
}

void DI_Close(){
//------------------ rodries --------------  
  motorthreadexit=true;
  LastAccess=0;
  motor_stopped=false;
//---------------- end rodries ------------  

	if(di_fd > 0){
		IOS_Close(di_fd);
	}
	di_fd = -1;

	DI_ReadDVDptr = NULL;
	DI_ReadDVDAsyncptr = NULL;
	state = DVD_INIT | DVD_NO_DISC;
}
	

#define COVER_CLOSED (*((uint32_t*)usrdata) & 0x2)

static int _cover_callback(int ret, void* usrdata){
	static int cur_state = 0;
	static int retry_count = MAX_RETRY;
	const int callback_table[] = {
		DVD_GETCOVER,
		DVD_WAITFORCOVERCLOSE,
		DVD_RESET,
		DVD_IDENTIFY,		// This one will complete when the drive is ready.
		DVD_READ_DISCID,
		0};
	const int return_table[] = {1,1,4,1,1};
	if(cur_state > 1)
		state &= ~DVD_NO_DISC;

	if(callback_table[cur_state]){
		if(ret == return_table[cur_state]){
			if(cur_state == 1 && COVER_CLOSED)	// Disc inside, skipping wait for cover.
				cur_state += 2;	
			else
				cur_state++; // If the previous callback succeeded, moving on to the next

			retry_count = MAX_RETRY;
		}
		else
		{
			retry_count--;
			if(retry_count < 0){		// Drive init failed for unknown reasons.
				retry_count = MAX_RETRY;
				cur_state = 0;
				state = DVD_UNKNOWN;
				return 0;
			}
		}
		_DI_SetCallback(callback_table[cur_state - 1], _cover_callback);

	}
	else		// Callback chain has completed OK. The drive is ready.
	{
		state = DVD_READY;
/*
		if(IOS_GetVersion() < 200)
    {
			state |= DVD_D0;
			DI_ReadDVDptr = _DI_ReadDVD_D0;
			DI_ReadDVDAsyncptr = _DI_ReadDVD_D0_Async;
		}
		else
		{
			state |= DVD_A8;
			DI_ReadDVDptr = _DI_ReadDVD_A8;
			DI_ReadDVDAsyncptr = _DI_ReadDVD_A8_Async;
		}
*/
		
		if(di_cb)
			di_cb(state,0);

		retry_count = MAX_RETRY;
		cur_state = 0;

	}
	return 0;
}

/* Get current status, will return the API status */
int DI_GetStatus(){
	return state;
}

void DI_SetInitCallback(di_callback cb){
	di_cb = cb;
}

void _DI_SetCallback(int ioctl_nr, ipccallback ipc_cb){
	if(!ipc_cb) return;

	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	memset(dic, 0x00, sizeof(dic));

	dic[0] = ioctl_nr << 24;
	dic[1] = (ioctl_nr == DVD_RESET)? 1 : 0;	// For reset callback. Dirty, I know...

	IOS_IoctlAsync(di_fd,ioctl_nr, dic, 0x20, outbuf, 0x20, ipc_cb, outbuf);

	//We're done with the buffer now.
	LWP_MutexUnlock(bufferMutex);
}

/*
Request an identification from the drive, returned in a DI_DriveID struct
*/
int DI_Identify(DI_DriveID* id){
	if(!id){
		errno = EINVAL;
		return -1;
	}

  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_IDENTIFY << 24;
	
	int ret = IOS_Ioctl(di_fd, DVD_IDENTIFY, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	memcpy(id,outbuf,sizeof(DI_DriveID));

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Returns the current error code on the drive.
yagcd has a pretty comprehensive list of possible error codes
*/
int DI_GetError(uint32_t* error){
	if(!error){
		errno = EINVAL;
		return -1;
	}

  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------	
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_GET_ERROR << 24;
	
	int ret = IOS_Ioctl(di_fd, DVD_GET_ERROR, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	*error = outbuf[0];		// Error code is returned as an int in the first four bytes of outbuf.

	LWP_MutexUnlock(bufferMutex);
	
	
	return (ret == 1)? 0 : -ret;		
}

/*
Reset the drive.
*/
int DI_Reset(){
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));
  //------- rodries ---------------
  //LastAccess=ticks_to_secs(gettime());
  //------- end rodries ---------------
	
	dic[0] = DVD_RESET << 24;
	dic[1] = 1;
	
	int ret = IOS_Ioctl(di_fd, DVD_RESET, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	


	return (ret == 1)? 0 : -ret;
}

/*
Main read function, basically just a wrapper to the function pointer.
Nicer then just exposing the pointer itself
*/
int DI_ReadDVD(void* buf, uint32_t len, uint32_t lba){
	int ret;
	if(DI_ReadDVDptr){

    //------- rodries ---------------
    CheckAccess();
    //------- end rodries ---------------
		// Wait for the lock. Doing it here saves me from doing it in all the read functions.
		while(LWP_MutexLock(bufferMutex));
		
		//------- rodries ---------------
		ret = ReadBlockFromCache(buf,len,lba);
		//------- end rodries ---------------
		//ret = DI_ReadDVDptr(buf,len,lba);
		LWP_MutexUnlock(bufferMutex);
		return ret;
	}
	return -1;
}

int DI_ReadDVDAsync(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb){
	int ret;
	if(DI_ReadDVDAsyncptr){
    //------- rodries ---------------
    CheckAccess();
    //------- end rodries ---------------
		while(LWP_MutexLock(bufferMutex));
		ret = DI_ReadDVDAsyncptr(buf,len,lba,ipc_cb);
		LWP_MutexUnlock(bufferMutex);
		return ret;
	}
	return -1;
}

/*
Unknown what this does as of now...
*/
int DI_ReadDVDConfig(uint32_t* val, uint32_t flag){
	if(!val){
		errno = EINVAL;
		return -1;
	}

  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_READ_CONFIG << 24;
	dic[1] = flag & 0x1;		// Update flag, val will be written if this is 1, val won't be written if it's 0.
	dic[2] = 0;					// Command will fail driveside if this is not zero.
	dic[3] = *val;
	
	int ret = IOS_Ioctl(di_fd, DVD_READ_CONFIG, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	*val = outbuf[0];
	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Read the copyright information on a DVDVideo
*/
int DI_ReadDVDCopyright(uint32_t* copyright){
	if(!copyright){
		errno = EINVAL;
		return -1;
	}

  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_READ_COPYRIGHT << 24;
	dic[1] = 0;
	
	int ret = IOS_Ioctl(di_fd, DVD_READ_COPYRIGHT, dic, 0x20, outbuf, 0x20);
	*copyright = *((uint32_t*)outbuf);		// Copyright information is returned as an int in the first four bytes of outbuf.

	if(ret == 2) errno = EIO;
	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Returns 0x800 bytes worth of Disc key
*/
int DI_ReadDVDDiscKey(void* buf){
	int ret;
	int retry_count = MAX_RETRY;

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

	
  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_DISCKEY << 24;
	dic[1] = 0;		// Unknown what this flag does.
	do{
		ret = IOS_Ioctl(di_fd, DVD_READ_DISCKEY, dic, 0x20, buf, 0x800);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
This function will read the initial sector on the DVD, which contains stuff like the booktype
*/
int DI_ReadDVDPhysical(void* buf){


	int ret;
	int retry_count = MAX_RETRY;

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_PHYSICAL << 24;
	dic[1] = 0;		// Unknown what this flag does.
	
	do{
		ret = IOS_Ioctl(di_fd, DVD_READ_PHYSICAL, dic, 0x20, buf, 0x800);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_ReportKey(int keytype, uint32_t lba, void* buf){

	if(!buf){
		errno = EINVAL;
		return -1;
	}
	
	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}

  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	while(LWP_MutexLock(bufferMutex));
	
	dic[0] = DVD_REPORTKEY << 24;
	dic[1] = keytype & 0xFF;
	dic[2] = lba;
	
	int ret = IOS_Ioctl(di_fd, DVD_REPORTKEY, dic, 0x20, buf, 0x20);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_GetCoverRegister(uint32_t* status){
	while(LWP_MutexLock(bufferMutex));

	memset(dic, 0x00, 0x20);

	int ret = IOS_Ioctl(di_fd, DVD_GETCOVER, dic, 0x20, outbuf, 0x20);
	if(ret == 2) errno = EIO;

	*status = outbuf[0];

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/* Internal function for controlling motor operations */
int _DI_SetMotor(int flag){
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));
  //------- rodries ---------------
  LastAccess=ticks_to_secs(gettime());
  //------- end rodries ---------------

	dic[0] = DVD_SET_MOTOR << 24;
	dic[1] = flag & 0x1;			// Eject flag.
	dic[2] = (flag >> 1) & 0x1;		// Don't use this flag, it kills the drive untill next reset.

	int ret = IOS_Ioctl(di_fd, DVD_SET_MOTOR, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;
  //------- rodries ---------------
  if(ret == 1) motor_stopped=true;
  //------- end rodries ---------------	
	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

/* Stop the drives motor, needs a reset afterwards for normal operation */
int DI_StopMotor(){
	return _DI_SetMotor(0);
}

/* Stop the motor, and eject the disc. Also needs a reset afterwards for normal operation */
int DI_Eject(){
	return _DI_SetMotor(1);
}

/* Warning, this will kill your drive untill the next reset. Will not respond to DI commands,
will not take in or eject the disc. Your drive will be d - e - d, dead.

I deem this function to be harmless, as normal operation will resume after a reset.
However, I am not liable for anyones drive exploding as a result from using this function.
*/
int DI_KillDrive(){
	return _DI_SetMotor(2);
}

int DI_ClosePartition() {
  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_CLOSE_PARTITION << 24;

	int ret = IOS_Ioctl(di_fd, DVD_CLOSE_PARTITION, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

int DI_OpenPartition(uint32_t offset)
{
	static ioctlv vectors[5] __attribute__((aligned(32)));
	static char certs[0x49e4] __attribute__((aligned(32)));
  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_OPEN_PARTITION << 24;
	dic[1] = offset;

	vectors[0].data = dic;
	vectors[0].len = 0x20;
	vectors[1].data = NULL;
	vectors[1].len = 0x2a4;
	vectors[2].data = NULL;
	vectors[2].len = 0;

	vectors[3].data = certs;
	vectors[3].len = 0x49e4;
	vectors[4].data = outbuf;
	vectors[4].len = 0x20;

	int ret = IOS_Ioctlv(di_fd, DVD_OPEN_PARTITION, 3, 2, vectors);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;

}

int DI_Read(void *buf, uint32_t size, uint32_t offset)
{

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){
		errno = EFAULT;
		return -1;
	}


  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_LOW_READ << 24;
	dic[1] = size;
	dic[2] = offset;

	int ret = IOS_Ioctl(di_fd, DVD_LOW_READ, dic, 0x20, buf, size);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

int DI_UnencryptedRead(void *buf, uint32_t size, uint32_t offset)
{
	int ret, retry_count = MAX_RETRY;

	if(!buf){
		errno = EINVAL;
		return -1;
	}

	if((uint32_t)buf & 0x1F){		// This only works with 32 byte aligned addresses!
		errno = EFAULT;
		return -1;
	}

  //------- rodries ---------------
  CheckAccess();
  //------- end rodries ---------------
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_UNENCRYPTED << 24;
	dic[1] = size;
	dic[2] = offset;

	do{	
		ret = IOS_Ioctl(di_fd, DVD_READ_UNENCRYPTED, dic, 0x20, buf, size);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2) errno = EIO;

	LWP_MutexUnlock(bufferMutex);

	return (ret == 1)? 0 : -ret;
}

int DI_ReadDiscID(uint64_t *id)
{
  //------- rodries ---------------
  CheckAccess();  
  //------- end rodries ---------------
	// Wait for the lock
	while(LWP_MutexLock(bufferMutex));

	dic[0] = DVD_READ_DISCID << 24;

	int ret = IOS_Ioctl(di_fd, DVD_READ_DISCID, dic, 0x20, outbuf, 0x20);

	if(ret == 2) errno = EIO;

	memcpy(id, outbuf, sizeof(*id));

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}
