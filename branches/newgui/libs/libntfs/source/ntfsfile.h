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

#ifndef _NTFSFILE_H
#define _NTFSFILE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ntfsinternal.h"
#include <sys/reent.h>

/**
 * ntfs_file_state - File state
 */
typedef struct _ntfs_file {
    ntfs_volume *vol;
    ntfs_inode *ni;
    ntfs_attr *data_na;
    int mode;
} ntfs_file_state;

/* Gekko devoptab file routines for NTFS-based devices */
extern int ntfs_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode);
extern int ntfs_close_r (struct _reent *r, int fd);
extern ssize_t ntfs_write_r (struct _reent *r,int fd, const char *ptr, size_t len);
extern ssize_t ntfs_read_r (struct _reent *r, int fd, char *ptr, size_t len);
extern off_t ntfs_seek_r (struct _reent *r, int fd, off_t pos, int dir);
extern int ntfs_fstat_r (struct _reent *r, int fd, struct stat *st);
extern int ntfs_ftruncate_r (struct _reent *r, int fd, off_t len);
extern int ntfs_fsync_r (struct _reent *r, int fd);

#endif /* _NTFSFILE_H */

