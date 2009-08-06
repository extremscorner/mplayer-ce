/**
 * ntfs_dir.c - devoptab directory routines for NTFS-based devices.
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
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
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
#include "ntfsdir.h"
#include "device.h"
#include <sys/dir.h>

#define STATE(x)    ((ntfs_dir_state*)(x)->dirStruct)

int ntfs_stat_r (struct _reent *r, const char *path, struct stat *st)
{
    ntfs_log_trace("path %s, st %p\n", path, st);

    ntfs_vd *vd = NULL;
    ntfs_inode *ni = NULL;
    
    // Get the volume descriptor for this path
    vd = ntfsGetVolume(path);
    if (!vd) {
        r->_errno = ENODEV;
        return -1;
    }
    
    // Lock
    ntfsLock(vd);
    
    // Find the entry
    ni = ntfsOpenEntry(vd, path);
    if (!ni) {
        r->_errno = errno;
        ntfsUnlock(vd);
        return -1;
    }
    
    // Get the entry stats
    int ret = ntfsStat(vd, ni, st);
    if (ret)
        r->_errno = errno;

    // Close the entry
    ntfsCloseEntry(vd, ni);
    
    return ret;
}

int ntfs_link_r (struct _reent *r, const char *existing, const char *newLink)
{
    ntfs_log_trace("existing %s, newLink %s\n", existing, newLink);
    
    // Relink the entry
    int ret = ntfsLink(ntfsGetVolume(existing), existing, newLink);
    if (ret)
        r->_errno = errno;
    
    return ret;
}

int ntfs_unlink_r (struct _reent *r, const char *name)
{
    ntfs_log_trace("name %s\n", name);

    ntfs_vd *vd = ntfsGetVolume(name);
	struct ntfs_device *dev = vd->vol->dev;

    // Unlink the entry
    int ret = ntfsUnlink(vd, name);
    if (ret)
        r->_errno = errno;
       
	// Sync
	printf("ntfs_unlink_r: dev->d_ops->sync(dev)\n");
	dev->d_ops->sync(dev);        
        
    return ret;
}

int ntfs_chdir_r (struct _reent *r, const char *name)
{
    ntfs_log_trace("name %s\n", name);
    
    ntfs_vd *vd = NULL;
    ntfs_inode *ni = NULL;
    
    // Get the volume descriptor for this path
    vd = ntfsGetVolume(name);
    if (!vd) {
        r->_errno = ENODEV;
        return -1;
    }

    // Lock
    ntfsLock(vd);
    
    // Find the directory
    ni = ntfsOpenEntry(vd, name);
    if (!ni) {
        ntfsUnlock(vd);
        r->_errno = ENOENT;
        return -1;
    }
    
    // Ensure that this directory is indeed a directory
    if (!(ni->mrec->flags && MFT_RECORD_IS_DIRECTORY)) {
        ntfsCloseEntry(vd, ni);
        ntfsUnlock(vd);
        r->_errno = ENOTDIR;
        return -1;
    }
    
    // Set the current directory
    vd->cwd_ni = ni;

    // Unlock
    ntfsUnlock(vd);
    
    return 0;
}

int ntfs_rename_r (struct _reent *r, const char *oldName, const char *newName)
{
    ntfs_log_trace("oldName %s, newName %s\n", oldName, newName);
    
    ntfs_vd *vd = NULL;
    ntfs_inode *ni = NULL;
    struct ntfs_device *dev;
    
    // Get the volume descriptor for this path
    vd = ntfsGetVolume(oldName);
    if (!vd) {
        r->_errno = ENODEV;
        return -1;
    }
    dev = vd->vol->dev;
    // You cannot rename between devices
    if(vd != ntfsGetVolume(newName)) {
        r->_errno = EXDEV;
        return -1;
    }

    // Lock
    ntfsLock(vd);
    
    // Check that there is no existing entry with the new name
    ni = ntfsOpenEntry(vd, newName);
    if (ni) {
        ntfsCloseEntry(vd, ni);
        ntfsUnlock(vd);
        r->_errno = EEXIST;
        return -1;
    }

    // Link the old entry with the new one
    if (ntfsLink(vd, oldName, newName)) {
        ntfsUnlock(vd);
        return -1;
    }
    
    // Unlink the old entry
    if (ntfsUnlink(vd, oldName)) {
        if (ntfsUnlink(vd, newName)) {
            ntfsUnlock(vd);
            return -1;
        }
    }
    
    // Sync
    dev->d_ops->sync(dev);
    
    // Unlock
    ntfsUnlock(vd);
    
    return 0;
}

int ntfs_mkdir_r (struct _reent *r, const char *path, int mode)
{
    ntfs_log_trace("path %s, mode %i\n", path, mode);
    
    ntfs_vd *vd = NULL;
    ntfs_inode *ni = NULL;
	struct ntfs_device *dev;    
    
    // Get the volume descriptor for this path
    vd = ntfsGetVolume(path);
    if (!vd) {
        r->_errno = ENODEV;
        return -1;
    }
    dev = vd->vol->dev;
    
    // Lock
    ntfsLock(vd);
    
    // Create the directory
    ni = ntfsCreate(vd, path, S_IFDIR, 0, NULL);
    if (!ni) {
        ntfsUnlock(vd);
        r->_errno = errno;
        return -1;
    }
    
    // Close the directory
    ntfsCloseEntry(vd, ni);
    
	// Sync
	dev->d_ops->sync(dev);        
    
    // Unlock
    ntfsUnlock(vd);
    
    return 0;
}

int ntfs_statvfs_r (struct _reent *r, const char *path, struct statvfs *buf)
{
    ntfs_log_trace("path %s, buf %p\n", path, buf);
    
    ntfs_vd *vd = NULL;
    s64 size;
    int delta_bits;
    
    // Get the volume descriptor for this path
    vd = ntfsGetVolume(path);
    if (!vd) {
        r->_errno = ENODEV;
        return -1;
    }
    
    // Lock
    ntfsLock(vd);
    
    // Zero out the stat buffer
    /*memset(buf, 0, sizeof(struct statvfs));*/
    
    // File system block size
    buf->f_bsize = vd->vol->cluster_size;
    
    // Fundamental file system block size
    buf->f_frsize = vd->vol->cluster_size;
    
    // Total number of blocks on file system in units of f_frsize
    buf->f_blocks = vd->vol->nr_clusters;
    
    // Free blocks available for all and for non-privileged processes
    size = MAX(vd->vol->free_clusters, 0);
    buf->f_bfree = buf->f_bavail = size;
    
    // Free inodes on the free space
    delta_bits = vd->vol->cluster_size_bits - vd->vol->mft_record_size_bits;
    if (delta_bits >= 0)
        size <<= delta_bits;
    else
        size >>= -delta_bits;
    
    // Number of inodes at this point in time
    buf->f_files = (vd->vol->mftbmp_na->allocated_size << 3) + size;
    
    // Free inodes available for all and for non-privileged processes
    size += vd->vol->free_mft_records;
    buf->f_ffree = buf->f_favail = MAX(size, 0);
    
    // File system id
    buf->f_fsid = vd->id;
    
    // Bit mask of f_flag values.
    buf->f_flag = (NVolReadOnly(vd->vol) ? ST_RDONLY : 0);
    
    // Maximum length of filenames
    buf->f_namemax = NTFS_MAX_NAME_LEN;
    
    // Unlock
    ntfsUnlock(vd);
    
    return 0;
}

/**
 * PRIVATE: Callback for directory enumeration
 */
int ntfs_readdir_filler (DIR_ITER *dirState, const ntfschar *name, const int name_len, const int name_type,
                         const s64 pos, const MFT_REF mref, const unsigned dt_type)
{
    ntfs_dir_state* dir = STATE(dirState);
    
    // Sanity check
    if (!dir || !dir->vd) {
        errno = EINVAL;
        return -1;
    }
    
    // Lock
    ntfsLock(dir->vd);
    
    // If we have a entry waiting to be fetched (dirnext()), then abort
    if (dir->current) {
        ntfsUnlock(dir->vd);
        return -1;
    }
    
    // Computer says no...
    if (name_type == FILE_NAME_DOS) {
        ntfsUnlock(dir->vd);
        return 0;
    }
    
    // Check that this entry can be enumerated (as described by the volume descriptor)
    if (MREF(mref) == FILE_root || MREF(mref) >= FILE_first_user || dir->vd->showSystemFiles) {
        
        // Convert the entry name to our current local and line it up for fetching
        if (ntfsUnicodeToLocal(name, name_len, &dir->current, 0) < 0) {
            ntfsUnlock(dir->vd);
            dir->current = NULL;
            return -1;
        }
        
    }
    
    // Unlock
    ntfsUnlock(dir->vd);
    
    return 0;
}

DIR_ITER *ntfs_diropen_r (struct _reent *r, DIR_ITER *dirState, const char *path)
{
    ntfs_log_trace("dirState %p, path %s\n", dirState, path);
    
    ntfs_dir_state* dir = STATE(dirState);
    
    // Get the volume descriptor for this path
    dir->vd = ntfsGetVolume(path);
    if (!dir->vd) {
        r->_errno = ENODEV;
        return NULL;
    }

    // Lock
    ntfsLock(dir->vd);
    
    // Find the directory
    dir->ni = ntfsOpenEntry(dir->vd, path);
    if (!dir->ni) {
        ntfsUnlock(dir->vd);
        r->_errno = ENOENT;
        return NULL;
    }
    
    // Ensure that this directory is indeed a directory
    if (!(dir->ni->mrec->flags && MFT_RECORD_IS_DIRECTORY)) {
        ntfsCloseEntry(dir->vd, dir->ni);
        ntfsUnlock(dir->vd);
        r->_errno = ENOTDIR;
        return NULL;
    }
    
    // Move to the first entry in the directory
    dir->position = 0;
    dir->current = NULL;
    ntfs_readdir(dir->ni, &dir->position, dirState, (ntfs_filldir_t)ntfs_readdir_filler);

    // Update entry times
    ntfsUpdateTimes(dir->vd, dir->ni, NTFS_UPDATE_ATIME);
    
    // Unlock
    ntfsUnlock(dir->vd);
    
    return dirState;
}

int ntfs_dirreset_r (struct _reent *r, DIR_ITER *dirState)
{
    ntfs_log_trace("dirState %p\n", dirState);
    
    ntfs_dir_state* dir = STATE(dirState);
    
    // Sanity check
    if (!dir || !dir->vd || !dir->ni) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(dir->vd);
    
    // Free the current entry (if any)
    if (dir->current)
        ntfs_free(dir->current);
    
    // Move to the first entry in the directory
    dir->position = 0;
    dir->current = NULL;
    ntfs_readdir(dir->ni, &dir->position, dirState, (ntfs_filldir_t)ntfs_readdir_filler);
    
    // Update entry times
    ntfsUpdateTimes(dir->vd, dir->ni, NTFS_UPDATE_ATIME);
    
    // Unlock
    ntfsUnlock(dir->vd);
    
    return 0;
}

int ntfs_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat)
{
    ntfs_log_trace("dirState %p, filename %p, filestat %p\n", dirState, filename, filestat);
    
    ntfs_dir_state* dir = STATE(dirState);
    ntfs_inode *ni = NULL;
    
    // Sanity check
    if (!dir || !dir->vd || !dir->ni) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(dir->vd);
    
    // Check that there is a entry waiting to be fetched
    if (!dir->current) {
        ntfsUnlock(dir->vd);
        r->_errno = ENOENT;
        return -1;
    }

    // Fetch the current entry
    strcpy(filename, dir->current);
    if(filestat != NULL) {
        ni = ntfsOpenEntry(dir->vd, dir->current);
        if (ni) {
            ntfsStat(dir->vd, ni, filestat);
            ntfsCloseEntry(dir->vd, ni);
        }
    }
    
    // Free the current entry
    ntfs_free(dir->current);
    dir->current = NULL;
    
    // Move to the next entry in the directory
    ntfs_readdir(dir->ni, &dir->position, dirState, (ntfs_filldir_t)ntfs_readdir_filler);
    
    // Update entry times
    ntfsUpdateTimes(dir->vd, dir->ni, NTFS_UPDATE_ATIME);
    
    // Unlock
    ntfsUnlock(dir->vd);
    
    return 0;
}

int ntfs_dirclose_r (struct _reent *r, DIR_ITER *dirState)
{
    ntfs_log_trace("dirState %p\n", dirState);
    
    ntfs_dir_state* dir = STATE(dirState);
    
    // Sanity check
    if (!dir || !dir->vd) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(dir->vd);
    
    // Free the current entry (if any)
    if (dir->current)
        ntfs_free(dir->current);
    
    // Close the directory (if open)
    if (dir->ni)
        ntfsCloseEntry(dir->vd, dir->ni);
    
    // Reset the directory state
    dir->ni = NULL;
    dir->position = 0;
    dir->current = NULL;
    
    // Unlock
    ntfsUnlock(dir->vd);
    
    return 0;
}
