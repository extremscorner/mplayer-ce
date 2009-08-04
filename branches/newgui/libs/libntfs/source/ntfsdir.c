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
#include <sys/dir.h>

#define STATE(x)    ((ntfs_dir_state*)(x)->dirStruct)
#define DEV_FD(x)   ((gekko_fd*)(x)->dev->d_private)

int ntfs_stat_r (struct _reent *r, const char *path, struct stat *st)
{
    ntfs_log_trace("path %s\n", path);

    // Get the entry stats
    int ret = ntfsStat(path, st);
    if (ret)
        r->_errno = errno;

    return ret;
}

int ntfs_link_r (struct _reent *r, const char *existing, const char *newLink)
{
    ntfs_log_trace("existing %s, newLink %s\n", existing, newLink);
    
    // Relink the entry
    int ret = ntfsLink(existing, newLink);
    if (ret)
        r->_errno = errno;
    
    return ret;
}

int ntfs_unlink_r (struct _reent *r, const char *name)
{
    ntfs_log_trace("name %s\n", name);
    
    // Unlink the entry
    int ret = ntfsUnlink(name);
    if (ret)
        r->_errno = errno;
    
    return ret;
}

int ntfs_chdir_r (struct _reent *r, const char *name)
{
    ntfs_log_trace("name %s\n", name);
    
    //...
    
    return 0;
}

int ntfs_rename_r (struct _reent *r, const char *oldName, const char *newName)
{
    ntfs_log_trace("oldName %s, newName %s\n", oldName, newName);
    
    ntfs_volume *vol = NULL;
    ntfs_inode *ni = NULL;
    
    // Get the volume for this path
    vol = ntfsGetVolumeFromPath(oldName);
    if (!vol) {
        r->_errno = ENODEV;
        return -1;
    }
    
    // You cannot rename between devices
    if(vol != ntfsGetVolumeFromPath(newName)) {
        r->_errno = EXDEV;
        return -1;
    }

    // Move the path pointers to the start of the actual path
    if (strchr(oldName, ':') != NULL) {
        oldName = strchr(oldName, ':') + 1;
    }
    if (strchr(newName, ':') != NULL) {
        newName = strchr(newName, ':') + 1;
    }
    if (strchr(oldName, ':') != NULL) {
        r->_errno = EINVAL;
        return -1;
    }
    if (strchr(newName, ':') != NULL) {
        r->_errno = EINVAL;
        return -1;
    }
    
    // Lock
    ntfsLock(DEV_FD(vol));
    
    // Check that there is no existing entry with the new name
    ni = ntfs_pathname_to_inode(vol, NULL, newName);
    if (ni) {
        ntfs_inode_close(ni);
        ntfsUnlock(DEV_FD(vol));
        r->_errno = EEXIST;
        return -1;
    }

    // Link the old entry with the new one
    if (ntfsLink(oldName, newName)) {
        ntfsUnlock(DEV_FD(vol));
        return -1;
    }
    
    // Unlink the old entry
    if (ntfsUnlink(oldName)) {
        if (ntfsUnlink(newName)) {
            ntfsUnlock(DEV_FD(vol));
            return -1;
        }
    }
    
    // Unlock
    ntfsUnlock(DEV_FD(vol));
    
    return 0;
}

int ntfs_mkdir_r (struct _reent *r, const char *path, int mode)
{
    ntfs_log_trace("path %s, mode %i\n", path, mode);
    
    // Create the directory
    int ret = ntfsCreate(path, S_IFDIR, 0, NULL);
    if (ret)
        r->_errno = errno;
    
    return ret;
}

int ntfs_statvfs_r (struct _reent *r, const char *path, struct statvfs *buf)
{
    ntfs_log_trace("path %s\n", path);
    
    ntfs_volume *vol = NULL;
    s64 size;
    int delta_bits;
    
    // Get the volume for this path
    vol = ntfsGetVolumeFromPath(path);
    if (!vol) {
        r->_errno = ENODEV;
        return -1;
    }
    
    // Lock
    ntfsLock(DEV_FD(vol));
    
    // Zero out the stat buffer
    //memset(buf, 0, sizeof(struct statvfs));
    
    // File system block size
    buf->f_bsize = vol->cluster_size;
    
    // Fundamental file system block size
    buf->f_frsize = vol->cluster_size;
    
    // Total number of blocks on file system in units of f_frsize
    buf->f_blocks = vol->nr_clusters;
    
    // Free blocks available for all and for non-privileged processes
    size = MAX(vol->free_clusters, 0);
    buf->f_bfree = buf->f_bavail = size;
    
    // Free inodes on the free space
    delta_bits = vol->cluster_size_bits - vol->mft_record_size_bits;
    if (delta_bits >= 0)
        size <<= delta_bits;
    else
        size >>= -delta_bits;
    
    // Number of inodes at this point in time
    buf->f_files = (vol->mftbmp_na->allocated_size << 3) + size;
    
    // Free inodes available for all and for non-privileged processes
    size += vol->free_mft_records;
    buf->f_ffree = buf->f_favail = MAX(size, 0);
    
    // File system id
    buf->f_fsid = DEV_FD(vol)->interface->ioType;
    
    // Bit mask of f_flag values.
    buf->f_flag = ((vol->state & NV_ReadOnly) ? ST_RDONLY : 0 );
    
    // Maximum length of filenames
    buf->f_namemax = NTFS_MAX_NAME_LEN;
    
    // Unlock
    ntfsUnlock(DEV_FD(vol));
    
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
    if (!dir || !dir->vol)
        return -1;
    
    // If we have a entry waiting to be fetched (dirnext()), then abort
    if (dir->current)
        return -1;

    // Computer says no...
    if (name_type == FILE_NAME_DOS)
        return 0;

    // Check that this entry can be enumerated under the current device driver settings
    if (MREF(mref) == FILE_root || MREF(mref) >= FILE_first_user || DEV_FD(dir->vol)->showSystemFiles) {
        
        // Convert the entry name to our current local and line it up for fetching
        if (ntfsUnicodeToLocal(name, name_len, &dir->current, 0) < 0) {
            ntfs_log_perror("Entry name decoding failed (inode %llu)", (unsigned long long)MREF(mref));
            dir->current = NULL;
            return -1;
        }
        
    }
    
    return 0;
}

DIR_ITER *ntfs_diropen_r (struct _reent *r, DIR_ITER *dirState, const char *path)
{
    ntfs_log_trace("path %s\n", path);
    
    ntfs_dir_state* dir = STATE(dirState);
    
    // Get the volume for this path
    dir->vol = ntfsGetVolumeFromPath(path);
    if (!dir->vol) {
        r->_errno = ENODEV;
        return NULL;
    }

    // Move the path pointer to the start of the actual path
    if (strchr(path, ':') != NULL) {
        path = strchr(path, ':') + 1;
    }
    if (strchr(path, ':') != NULL) {
        r->_errno = EINVAL;
        return NULL;
    }
    
    // Lock
    ntfsLock(DEV_FD(dir->vol));
    
    // Find the directory
    dir->ni = ntfs_pathname_to_inode(dir->vol, NULL, path);
    if (!dir->ni) {
        ntfsUnlock(DEV_FD(dir->vol));
        r->_errno = ENOENT;
        return NULL;
    }

    // Ensure that this is actually a directory
    if (!(dir->ni->mrec->flags && MFT_RECORD_IS_DIRECTORY)) {
        ntfs_inode_close(dir->ni);
        ntfsUnlock(DEV_FD(dir->vol));
        r->_errno = ENOTDIR;
        return NULL;
    }
    
    // Move to the first entry in the directory
    dir->position = 0;
    dir->current = NULL;
    ntfs_readdir(dir->ni, &dir->position, dirState, (ntfs_filldir_t)ntfs_readdir_filler);

    // Update entry times
    ntfsUpdateTimes(dir->ni, NTFS_UPDATE_ATIME);
    
    // Unlock
    ntfsUnlock(DEV_FD(dir->vol));
    
    return dirState;
}

int ntfs_dirreset_r (struct _reent *r, DIR_ITER *dirState)
{
    ntfs_log_trace("\n");
    
    ntfs_dir_state* dir = STATE(dirState);
    
    // Sanity check
    if (!dir->vol || !dir->ni) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(DEV_FD(dir->vol));
    
    // Free the current entry (if any)
    if (dir->current)
        ntfs_free(dir->current);
    
    // Move to the first entry in the directory
    dir->position = 0;
    dir->current = NULL;
    ntfs_readdir(dir->ni, &dir->position, dirState, (ntfs_filldir_t)ntfs_readdir_filler);
    
    // Update entry times
    ntfsUpdateTimes(dir->ni, NTFS_UPDATE_ATIME);
    
    // Unlock
    ntfsUnlock(DEV_FD(dir->vol));
    
    return 0;
}

int ntfs_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat)
{
    ntfs_log_trace("\n");
    
    ntfs_dir_state* dir = STATE(dirState);
    
    // Sanity check
    if (!dir->vol || !dir->ni) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(DEV_FD(dir->vol));
    
    // Check that there is a entry waiting to be fetched
    if (!dir->current) {
        ntfsUnlock(DEV_FD(dir->vol));
        r->_errno = ENOENT;
        return -1;
    }

    // Fetch the current entry
    strcpy(filename, dir->current);
    if(filestat != NULL) {
        ntfsStat(dir->current, filestat);
    }
    
    // Free the current entry
    ntfs_free(dir->current);
    dir->current = NULL;
    
    // Move to the next entry in the directory
    ntfs_readdir(dir->ni, &dir->position, dirState, (ntfs_filldir_t)ntfs_readdir_filler);
    
    // Update entry times
    ntfsUpdateTimes(dir->ni, NTFS_UPDATE_ATIME);
    
    // Unlock
    ntfsUnlock(DEV_FD(dir->vol));
    
    return 0;
}

int ntfs_dirclose_r (struct _reent *r, DIR_ITER *dirState)
{
    ntfs_log_trace("\n");
    
    ntfs_dir_state* dir = STATE(dirState);
    
    // Sanity check
    if (!dir->vol) {
        r->_errno = EBADF;
        return -1;
    }
    
    // Lock
    ntfsLock(DEV_FD(dir->vol));
    
    // Free the current entry (if any)
    if (dir->current)
        ntfs_free(dir->current);
    
    // Close the directory (if open)
    if (dir->ni)
        ntfs_inode_close(dir->ni);
    
    // Reset the directory state
    dir->ni = NULL;
    dir->position = 0;
    dir->current = NULL;
    
    // Unlock
    ntfsUnlock(DEV_FD(dir->vol));
    
    return 0;
}
