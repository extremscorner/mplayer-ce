/**
 * ntfsinternal.h - Internal support routines for NTFS-based devices.
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

#ifndef _NTFSINTERNAL_H
#define _NTFSINTERNAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "types.h"
#include "compat.h"
#include "logging.h"
#include "layout.h"
#include "device.h"
#include "volume.h"
#include "dir.h"
#include "inode.h"
#include "attrib.h"
#include "unistr.h"
#include "gekko_io.h"

#include <gccore.h>
#include <ogc/disc_io.h>
#include <sys/iosupport.h>

/**
 * INTERFACE_ID - Disc interface identifier
 */
typedef struct _INTERFACE_ID {
    const char *name; 
    const DISC_INTERFACE *interface;
} INTERFACE_ID;

/* All know disc interfaces */
extern const INTERFACE_ID ntfs_disc_interfaces[];

/* Lock gekko NTFS device driver */
static inline void ntfsLock (gekko_fd *fd)
{
    LWP_MutexLock(fd->lock);
}

/* Unlock gekko NTFS device driver */
static inline void ntfsUnlock (gekko_fd *fd)
{
    LWP_MutexUnlock(fd->lock);
}

/* Miscellaneous helper/support routines */
ntfs_volume *ntfsGetVolumeFromPath (const char *path);
int ntfsUnicodeToLocal (const ntfschar *ins, const int ins_len, char **outs, int outs_len);
int ntfsLocalToUnicode (const char *ins, ntfschar **outs);
int ntfsCreate (const char *path, dev_t type, dev_t dev, const char *target);
int ntfsLink (const char *old_path, const char *new_path);
int ntfsUnlink (const char *path);
int ntfsStat (const char *path, struct stat *st);
void ntfsUpdateTimes (ntfs_inode *ni, ntfs_time_update_flags mask);

/* Gekko devoptab related routines */
const devoptab_t *ntfsDeviceOpTab (void);
const devoptab_t *ntfsGetDeviceOpTab (const char *name);

#endif /* _NTFSINTERNAL_H */
