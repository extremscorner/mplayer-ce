//tinysmbmount v.0.3 beta, scip, rodries
//read only, test code
//improve it, share it

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/iosupport.h>
#include <network.h>
#include <malloc.h>
#include <fcntl.h>
#include <ogcsys.h>
#include <smb.h>

#include "tinysmbmount.h"


typedef struct{	
	SMBFILE handle;
  u32 offset;
  u32 len;
} SMBFILESTRUCT;

typedef struct{
	SMBDIRENTRY * filelist;
	int position;
	int count;
} SMBDIRSTATESTRUCT;


//global
SMBCONN smbconn;

static mutex_t __smb_mutex = 0;
//static unsigned int bufferMutex = 0;

static inline void __smb_lock_init(){
	LWP_MutexInit(&__smb_mutex, false);
}

static inline void __smb_lock_deinit(){
	LWP_MutexDestroy(__smb_mutex);
}

static inline void __smb_lock(){
	while(LWP_MutexLock(__smb_mutex));
}

static inline void __smb_unlock(){
	LWP_MutexUnlock(__smb_mutex);
}



///////////////////////////////////////////
//      CACHE FUNCTION DEFINITIONS       //
///////////////////////////////////////////
#define CACHE_FREE 0xFFFFFFFF

#define SMB_BUFFERSIZE			7236

typedef struct
{
	u32 offset;
	u32 last_used;
  SMBFILESTRUCT *file;	
	void *ptr;
} smb_cache_page;

smb_cache_page *SMBReadAheadCache=NULL;
u32 SMB_RA_pages=0;

u32 gettick();


void DestroySMBReadAheadCache();
void SMBEnableReadAhead(u32 pages);
int ReadSMBFromCache(void *buf, int len,SMBFILESTRUCT *file);

///////////////////////////////////////////
//    END CACHE FUNCTION DEFINITIONS     //
///////////////////////////////////////////

///////////////////////////////////////////
//         CACHE FUNCTIONS              //
///////////////////////////////////////////

void DestroySMBReadAheadCache()
{
	int i;
	if(SMBReadAheadCache==NULL) return;
	for(i = 0; i < SMB_RA_pages; i++)
	{
		free(SMBReadAheadCache[i].ptr);	 
	}
	free(SMBReadAheadCache);
	SMBReadAheadCache=NULL;
	SMB_RA_pages=0;
}

void SMBEnableReadAhead(u32 pages)
{
 	int i,j;
  
 	DestroySMBReadAheadCache();

 	if(pages==0) return; 
  
  
	SMB_RA_pages=pages;
	SMBReadAheadCache=(smb_cache_page *)malloc(sizeof(smb_cache_page) * SMB_RA_pages);
	if(SMBReadAheadCache==NULL)return;
	for(i = 0; i < SMB_RA_pages; i++)
	{
		SMBReadAheadCache[i].offset=CACHE_FREE;	 
		SMBReadAheadCache[i].last_used=0;	 
		SMBReadAheadCache[i].file=NULL;
		SMBReadAheadCache[i].ptr=memalign(32, SMB_BUFFERSIZE);
		if(SMBReadAheadCache[i].ptr==NULL)
		{
			for(j=i-1;j>=0;j--)
			if(SMBReadAheadCache[j].ptr)free(SMBReadAheadCache[j].ptr);
			free(SMBReadAheadCache);
			SMBReadAheadCache=NULL;
			return;
		}
		memset(SMBReadAheadCache[i].ptr, 0, SMB_BUFFERSIZE);
	}
}

void ClearSMBFileCache(SMBFILESTRUCT *file)  // clear cache from file (clear if you write)
{
  int i;
	for(i = 0; i < SMB_RA_pages; i++)
	{
	   if(SMBReadAheadCache[i].file==file)
	   {
      SMBReadAheadCache[i].offset=CACHE_FREE;
      SMBReadAheadCache[i].last_used=0;
      SMBReadAheadCache[i].file=NULL;	     
     }
  }
}

int ReadSMBFromCache(void *buf, int len,SMBFILESTRUCT *file)
{
	int retval;
	int i,leastUsed,rest;
  u32 new_offset;
	if(SMBReadAheadCache==NULL) 
  {	__smb_lock();
    if(SMB_ReadFile(buf, len, file->offset, file->handle)<=0){ 
    	__smb_unlock();
    	return -1; 
    }
    __smb_unlock();
    return 0;
  } 
  new_offset=file->offset;
  rest=len;
	leastUsed=0;
	for(i = 0; i < SMB_RA_pages; i++)
	{
	   if(SMBReadAheadCache[i].file==file)
	   {
	     if( (file->offset >= SMBReadAheadCache[i].offset) && (file->offset < (SMBReadAheadCache[i].offset+SMB_BUFFERSIZE)) )
	     {
  			 if((file->offset+len) <= (SMBReadAheadCache[i].offset+SMB_BUFFERSIZE)) 
	   		 {
				  SMBReadAheadCache[i].last_used=gettick();
				  memcpy(buf,SMBReadAheadCache[i].ptr + (file->offset - SMBReadAheadCache[i].offset) , len);
				  return 0;
			   }			   
			   else
	   		 {
	   		  int buffer_used;
				  SMBReadAheadCache[i].last_used=gettick();
				  buffer_used=(SMBReadAheadCache[i].offset+SMB_BUFFERSIZE)-file->offset;
				  memcpy(buf,SMBReadAheadCache[i].ptr + (file->offset - SMBReadAheadCache[i].offset) , buffer_used);
          buf+=buffer_used;
				  rest=len-buffer_used;
				  new_offset=SMBReadAheadCache[i].offset+SMB_BUFFERSIZE;
				  i++;
          break;
			   }
			   
			 }
		 }
  	 if((SMBReadAheadCache[i].last_used < SMBReadAheadCache[leastUsed].last_used))
			leastUsed = i;	
	}

	for(; i < SMB_RA_pages; i++)
	{
		if((SMBReadAheadCache[i].last_used < SMBReadAheadCache[leastUsed].last_used))
			leastUsed = i;	
	}

	__smb_lock();
	retval=SMB_ReadFile(SMBReadAheadCache[leastUsed].ptr, SMB_BUFFERSIZE, new_offset, file->handle);
	__smb_unlock();
	if(retval<=0)
  {
    SMBReadAheadCache[leastUsed].offset=CACHE_FREE;
    SMBReadAheadCache[leastUsed].last_used=0;
    SMBReadAheadCache[leastUsed].file=NULL;
    return -1;
  }
	
	SMBReadAheadCache[leastUsed].offset=new_offset;	 
	SMBReadAheadCache[leastUsed].last_used=gettick();
	SMBReadAheadCache[leastUsed].file=file;
	memcpy(buf, SMBReadAheadCache[leastUsed].ptr, rest);	 

	return 0;
}


///////////////////////////////////////////
//         END CACHE FUNCTIONS           //
///////////////////////////////////////////




void smbpath(const char *path, char *absolute){

	if(strchr(path, ':') != NULL) { //strstr(path,"smb:")==path ?!
			//absolute = (char*) malloc(strlen(path)+1-5);
			strcpy(absolute,path+5);
	}else{
			char current[256];
			getcwd(current, 256);
			//absolute = (char*) malloc(strlen(current)+strlen(path)+1-5);
			absolute[0]='\0';
			strcat(absolute,current+5);
			strcat(absolute,path);
	}

	//TODO: remove ./ ../

	int i;
	for(i=0; i<strlen(absolute); i++) if(absolute[i]=='/') absolute[i]='\\';

	//return absolute;
}




//FILE IO //////////////

int __smb_open(struct _reent *r, void *fileStruct, const char *path, int flags, int mode){
	SMBFILESTRUCT *file = (SMBFILESTRUCT*)fileStruct;
	
	//printf("\n\tSMB_OPEN:%s\n",path);
	
	/*
	if (strchr (path, ':') != NULL) {
		path = strchr (path, ':') + 1;
	}
	if (strchr (path, ':') != NULL) {
		r->_errno = EINVAL;
		return -1;
	}*/
	
	__smb_lock();
	
	char fixedpath[256];
	smbpath(path,fixedpath);
	
	//printf("\n\tSMB_OPEN_fixedpath:%s",fixedpath);
	
	// lenght?
	SMBDIRENTRY smbdir;
	int found = SMB_FindFirst (fixedpath, SMB_SRCH_READONLY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN, &smbdir, smbconn); 
	SMB_FindClose (smbconn);
	
	if (found != SMB_SUCCESS){
				r->_errno = ENOENT;
				__smb_unlock();
				return -1;
	}
	
	/*if(SMB_PathInfo(fixedpath, &smbdir, smbconn) != SMB_SUCCESS)
  {
  	__smb_unlock();
    r->_errno = ENOENT;
    return -1;
  }	*/
	
	//printf("0");

	file->handle = SMB_OpenFile(fixedpath, SMB_OPEN_READING, SMB_OF_OPEN, smbconn);
	if(!file->handle){ 
		__smb_unlock();
		return -1; 
	}
		
	//printf("1");
	
	file->offset = 0;
	file->len = smbdir.size_low;
	
	__smb_unlock();
	return 0;

}

int __smb_seek(struct _reent *r, int fd, int pos, int dir){
	SMBFILESTRUCT *file = (SMBFILESTRUCT*)fd;
	int position;
	
	__smb_lock();
	if (file == NULL)	{ 
		r->_errno = EBADF; 
		__smb_unlock();
		return -1; 
	}
	
	switch(dir) {
        case SEEK_SET:
            position = pos;
            break;
        case SEEK_CUR:
            position = file->offset + pos;
            break;
        case SEEK_END:
            position = file->len + pos;
            break;
				default:
						r->_errno = EINVAL;
						__smb_unlock();
						return -1;
  }
  
	if (((pos > 0) && (position < 0)) || (position > file->len)) { r->_errno = EOVERFLOW; __smb_unlock(); return -1; }
	if (position < 0) { r->_errno = EINVAL; __smb_unlock(); return -1; }

	
	// Save position
	file->offset = position;
	__smb_unlock();
	return position;
}


int __smb_read(struct _reent *r, int fd, char *ptr, int len){
	SMBFILESTRUCT *file = (SMBFILESTRUCT*)fd; 
	
	//__smb_lock();
	if (file == NULL)	 { r->_errno = EBADF; /*__smb_unlock();*/ return -1; }
	
	if (file->offset >= file->len) { //>
		r->_errno = EOVERFLOW;
		//__smb_unlock();
		return 0;
	}
	
	if (len + file->offset > file->len) {
		r->_errno = EOVERFLOW; //remove?? len always 1024?
		len = file->len - file->offset;
	}
	
	if (len <= 0) {
		//__smb_unlock();
		return 0;
	}
	
	/* 
	if(len>SMB_MAX_BUFFERSIZE)
	temp = memset(len)
	while(len>0)
	SMB_ReadFile(temp, len, file->offset, file->handle);
	memcpy(temp,ptr)
	free(temp)
	*/
	//len is always <=1024 here // devoptab takes care of that? // no SMB_MAX_BUFFERSIZE problem
	//printf("\nLen: %d\n",len);
	//__smb_lock();
	
	
	//SMB_ReadFile(ptr, len, file->offset, file->handle);
	if (ReadSMBFromCache(ptr, len, file) < 0){
		r->_errno = EOVERFLOW;
		//__smb_unlock();
		return -1;
	}
	
	//ReadSMBFromCache(ptr, len, file);
	
	file->offset += len;        
  
  //__smb_unlock();      
  return len;

}

int __smb_close(struct _reent *r, int fd){
	SMBFILESTRUCT *file = (SMBFILESTRUCT*)fd;
	
	__smb_lock();
	
	SMB_CloseFile(file->handle);
	
	file->len = 0;
	file->offset = 0;
	
	__smb_unlock();
	return 0;
}


//DIR IO //////////////

int __smb_chdir(struct _reent *r, const char *path) {

//only check for path size here?
//dir exist check?

return 0;
}

DIR_ITER* __smb_diropen(struct _reent *r, DIR_ITER *dirState, const char *path) {
	SMBDIRSTATESTRUCT* dir = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);

	//ENAMETOOLONG ?
	
	__smb_lock();
	
	char fixedpath[256];
	smbpath(path,fixedpath);
	
	SMBDIRENTRY smbdir;
	
	int found = SMB_FindFirst(fixedpath, SMB_SRCH_DIRECTORY | SMB_SRCH_READONLY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN, &smbdir, smbconn); 
	SMB_FindClose(smbconn);
	
	if (found != SMB_SUCCESS){
				r->_errno = ENOENT;
				__smb_unlock();
				return NULL;
	}

	//if(!(smbdir.attributes>>4 & 1u)){ r->_errno = ENOTDIR; return NULL; } 
	// xx010111 => xx-file-folder-volume-system-hidden-readonly // 16-23


	//get list	
	strcat(fixedpath,"\\*");
	dir->position=0;
	dir->count=0;
	
	if( SMB_FindFirst(fixedpath, SMB_SRCH_DIRECTORY | SMB_SRCH_READONLY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN, &smbdir, smbconn) == SMB_SUCCESS){
		
		dir->filelist = (SMBDIRENTRY*) malloc(sizeof(SMBDIRENTRY));
		//memcpy(dir->filelist, &smbdir, sizeof(SMBDIRENTRY));
		dir->filelist[0] = smbdir;
		dir->count++;
		
		while(SMB_FindNext(&smbdir, smbconn)==SMB_SUCCESS){
			 dir->filelist = (SMBDIRENTRY*) realloc(dir->filelist, (dir->count+1) * sizeof(SMBDIRENTRY));
			 //memcpy(dir->filelist+(dir->count*sizeof(SMBDIRENTRY)), &smbdir, sizeof(SMBDIRENTRY));
			 dir->filelist[dir->count] = smbdir;
			 //printf("\t%s\n",dir->filelist[dir->count].name);
			 dir->count++;
		}
		
	}

	SMB_FindClose(smbconn);
	
	__smb_unlock();
	return (DIR_ITER*) dir;
}

int __smb_dirnext(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat) {
	SMBDIRSTATESTRUCT* dir = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	
	__smb_lock();
	
	if(dir->position >= dir->count){ __smb_unlock(); return -1; }
	
	if (filestat != NULL){
		filestat->st_mode = (dir->filelist[dir->position].attributes>>4 & 1u) ? S_IFDIR : S_IFREG;
    filestat->st_size = dir->filelist[dir->position].size_low;
	}
	
	strcpy(filename, dir->filelist[dir->position].name);
	
	dir->position++;
	
	__smb_unlock();
	return 0;
}

int __smb_dirreset(struct _reent *r, DIR_ITER *dirState) {
	SMBDIRSTATESTRUCT* dir = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	
	__smb_lock();
	
	dir->position = 0;
	
	__smb_unlock();
	return 0;
}

int __smb_dirclose(struct _reent *r, DIR_ITER *dirState) {
  SMBDIRSTATESTRUCT* dir = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	
	__smb_lock();
	
	if(dir->filelist!=NULL) free(dir->filelist);
	dir->position = 0;
	dir->count = 0;
	
	__smb_unlock();
	return 0;
}

//new
int __smb_fstat (struct _reent *r, int fd, struct stat *st){
  SMBFILESTRUCT *file = (SMBFILESTRUCT*)fd;
  
  __smb_lock();
  
	if (file == NULL)	 { r->_errno = EBADF; __smb_unlock(); return -1; }

	st->st_size = file->len;
	//st->st_ino =
	
  __smb_unlock();
	return 0;
}

int __smb_stat (struct _reent *r, const char *path, struct stat *st) {
	
	__smb_lock();
	
	char fixedpath[256];
	smbpath(path,fixedpath);
	
	SMBDIRENTRY smbdir;
	
	int found = SMB_FindFirst(fixedpath, SMB_SRCH_DIRECTORY | SMB_SRCH_READONLY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN, &smbdir, smbconn); 
	SMB_FindClose(smbconn);
	
	if (found != SMB_SUCCESS){
				r->_errno = ENOENT;
				__smb_unlock();
				return -1;
	}
	
	/*if(SMB_PathInfo(fixedpath, &smbdir, smbconn) != SMB_SUCCESS){
    __smb_unlock();
    r->_errno = ENOENT;
    return -1;
  }*/
	
	st->st_mode = (smbdir.attributes>>4 & 1u) ? S_IFDIR : S_IFREG;
  st->st_size = smbdir.size_low;
	
	__smb_unlock();
	return 0;
}


//SMB DEVOPTAB //////////////

const devoptab_t dotab_smb = {
	"smb",		// device name
	sizeof(SMBFILESTRUCT),		// size of file structure
	__smb_open,		// device open
	__smb_close,	// device close
	NULL,					// device write
	__smb_read,		// device read
	__smb_seek,			// device seek
	__smb_fstat,			// device fstat
	__smb_stat,			// device stat
	NULL,			// device link
	NULL,			// device unlink
	__smb_chdir,			// device chdir
	NULL,			// device rename
	NULL,			// device mkdir
	sizeof(SMBDIRSTATESTRUCT),		// dirStateSize
	__smb_diropen,			// device diropen_r
	__smb_dirreset,			// device dirreset_r
	__smb_dirnext,			// device dirnext_r
	__smb_dirclose,			// device dirclose_r
	NULL			// device statvfs_r
};



bool smbInit(SMBCONFIG *config, bool setAsDefaultDevice){

	if (if_config(NULL, NULL, NULL, true) < 0) return false;
	
	if (SMB_Connect(&smbconn, config->user, config->pass, config->share, config->ip, config->port) != SMB_SUCCESS) return false;	
	
	__smb_lock_init();
	
	AddDevice(&dotab_smb);
	
	SMBEnableReadAhead(32);
	
	if (setAsDefaultDevice) chdir("smb:/");
	
	return true;
}

bool smbInitDefault(SMBCONFIG *config){ return smbInit(config,true); }

void smbUnmount(){
	__smb_lock();
  SMB_Close(smbconn);
  DestroySMBReadAheadCache();
  __smb_unlock();
  __smb_lock_deinit();
}
