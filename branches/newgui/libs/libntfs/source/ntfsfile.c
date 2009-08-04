/**
 * ntfsfile.c - devoptab file routines for NTFS-based devices.
 *
 * Copyright (c) 2009 Rhys "Shareese" Koedijk
 * Copyright (c) 2006 Michael "Chishm" Chisholm
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ntfsinternal.h"
#include "ntfsfile.h"

#define STATE(x)    ((ntfs_file_state*)x)
#define DEV_FD(x)   ((gekko_fd*)(x)->dev->d_private)

int ntfs_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
    printf("ntfs_open_r\n");
    
    ntfs_file_state* file = STATE(fileStruct);
    
    // Get the volume for this path
    file->vol = ntfsGetVolumeFromPath(path);
    if (!file->vol) {
        r->_errno = ENODEV;
        return -1;
    }
    
    // Move the path pointer to the start of the actual path
    if (strchr(path, ':') != NULL) {
        path = strchr(path, ':') + 1;
    }
    if (strchr(path, ':') != NULL) {
        r->_errno = EINVAL;
        return -1;
    }
    
    // Lock
    ntfsLock(DEV_FD(file->vol));
    
    // Find the file
    file->ni = ntfs_pathname_to_inode(file->vol, NULL, path);
    if (!file->ni) {
        ntfsUnlock(DEV_FD(file->vol));
        r->_errno = ENOENT;
        return -1;
    }
    
    // Unlock
    ntfsUnlock(DEV_FD(file->vol));
    
    return (int)fileStruct;
}

int ntfs_close_r (struct _reent *r, int fd)
{
    printf("ntfs_close_r\n");
    
    ntfs_file_state* file = STATE(fd);
    
    // Sanity check
    if (!file->vol) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(DEV_FD(file->vol));
    
    // Close the file data attribute (if open)
    if (file->data_na)
        ntfs_attr_close(file->data_na);
    
    // Close the file (if open)
    if (file->ni)
        ntfs_inode_close(file->ni);
    
    // Reset the file state
    file->ni = NULL;
    file->data_na = NULL;
    file->mode = 0;
    
    // Unlock
    ntfsUnlock(DEV_FD(file->vol));
    
    return 0;
}

ssize_t ntfs_write_r (struct _reent *r,int fd, const char *ptr, size_t len)
{
    printf("ntfs_write_r\n");
    ssize_t written = 0;
    
    //...
    
    return written;
}

ssize_t ntfs_read_r (struct _reent *r, int fd, char *ptr, size_t len)
{
    printf("ntfs_read_r\n");
    ssize_t read = 0;
    
    //...
    
    return read;
}

off_t ntfs_seek_r (struct _reent *r, int fd, off_t pos, int dir)
{
    printf("ntfs_seek_r\n");
    off_t position = 0;
    
    //...
    
    return position;
}

int ntfs_fstat_r (struct _reent *r, int fd, struct stat *st)
{
    printf("ntfs_fstat_r\n");
    //...
    
    return 0;
}

int ntfs_ftruncate_r (struct _reent *r, int fd, off_t len)
{
    printf("ntfs_ftruncate_r\n");
    //...
    
    return 0;
}

int ntfs_fsync_r (struct _reent *r, int fd)
{
    printf("ntfs_fsync_r\n");
    //...
    
    return 0;
}
