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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
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

int ntfs_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
    ntfs_log_trace("fileStruct %p, path %s, flags %i, mode %i\n", fileStruct, path, flags, mode);
    
    ntfs_file_state* file = STATE(fileStruct);
    
    // Get the volume descriptor for this path
    file->vd = ntfsGetVolume(path);
    if (!file->vd) {
        r->_errno = ENODEV;
        return -1;
    }

    // Lock
    ntfsLock(file->vd);
    
    // Determine which mode the file is opened for
    file->flags = flags;
    if ((flags & 0x03) == O_RDONLY) {
        file->read = true;
        file->write = false;
        file->append = false;
    } else if ((flags & 0x03) == O_WRONLY) {
        file->read = false;
        file->write = true;
        file->append = (flags & O_APPEND);
    } else if ((flags & 0x03) == O_RDWR) {
        file->read = true;
        file->write = true;
        file->append = (flags & O_APPEND);
    } else {
        r->_errno = EACCES;
        ntfsUnlock(file->vd);
        return -1;
    }
    
    // Try and find the file and (if found) ensure that it is not a directory
    file->ni = ntfsOpenEntry(file->vd, path);
    if (file->ni && (file->ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)) {
        ntfsCloseEntry(file->vd, file->ni);
        ntfsUnlock(file->vd);
        r->_errno = EISDIR;
        return -1;
    }
    
    // Are we creating this file?
    if (flags & O_CREAT) {
        
        // The file SHOULD NOT exist if we are creating it
        if (file->ni && (flags & O_EXCL)) {
            ntfsCloseEntry(file->vd, file->ni);
            ntfsUnlock(file->vd);
            r->_errno = EEXIST;
            return -1;
        }
        
        //...
        
    // Else we must be opening it
    } else {
       
        // The file SHOULD exist if we are creating it
        if (!file->ni) {
            ntfsUnlock(file->vd);
            r->_errno = ENOENT;
            return -1;
        }
        
    }
    
    // Unlock
    ntfsUnlock(file->vd);
    
    return (int)fileStruct;
}

int ntfs_close_r (struct _reent *r, int fd)
{
    ntfs_log_trace("fd %p\n", fd);
    
    ntfs_file_state* file = STATE(fd);
    
    // Sanity check
    if (!file->vd) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(file->vd);
    
    // Close the file data attribute (if open)
    if (file->data_na)
        ntfs_attr_close(file->data_na);
    
    // Close the file (if open)
    if (file->ni)
        ntfsCloseEntry(file->vd, file->ni);
    
    // Reset the file state
    file->ni = NULL;
    file->data_na = NULL;
    file->flags = 0;
    file->read = false;
    file->write = false;
    file->append = false;
    file->pos = 0;
    file->len = 0;
    
    // Unlock
    ntfsUnlock(file->vd);
    
    return 0;
}

ssize_t ntfs_write_r (struct _reent *r, int fd, const char *ptr, size_t len)
{
    ntfs_log_trace("fd %p, ptr %p, len %Li\n", fd, ptr, len);
    
    ssize_t written = 0;
    
    //...
    
    return written;
}

ssize_t ntfs_read_r (struct _reent *r, int fd, char *ptr, size_t len)
{
    ntfs_log_trace("fd %p, ptr %p, len %Li\n", fd, ptr, len);
    
    ssize_t read = 0;
    
    //...
    
    return read;
}

off_t ntfs_seek_r (struct _reent *r, int fd, off_t pos, int dir)
{
    ntfs_log_trace("fd %p, pos %Li, dir %i\n", fd, pos, dir);
    
    off_t position = 0;
    
    //...
    
    return position;
}

int ntfs_fstat_r (struct _reent *r, int fd, struct stat *st)
{
    ntfs_log_trace("fd %p\n", fd);
    
    //...
    
    return 0;
}

int ntfs_ftruncate_r (struct _reent *r, int fd, off_t len)
{
    ntfs_log_trace("fd %p, len %Li\n", fd, len);
    
    //...
    
    return 0;
}

int ntfs_fsync_r (struct _reent *r, int fd)
{
    ntfs_log_trace("fd %p\n", fd);
    
    //...
    
    return 0;
}
