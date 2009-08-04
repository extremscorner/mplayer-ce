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

#include <gccore.h>
#include <ogc/disc_io.h>
#include <sys/iosupport.h>

/**
* ntfs_atime_t - File access time state
*/
typedef enum {
    ATIME_ENABLED,
    ATIME_DISABLED,
    ATIME_RELATIVE
} ntfs_atime_t;

/**
 * ntfs_vd - NTFS volume descriptor
 */
typedef struct _ntfs_vd {
    ntfs_volume *vol;
    mutex_t lock;
    s64 id;
    u32 flags;
    u16 uid;
    u16 gid;
    u16 fmask;
    u16 dmask;
    ntfs_atime_t atime;
    bool showSystemFiles;
    ntfs_inode *cwd_ni;
} ntfs_vd;

/**
 * INTERFACE_ID - Disc interface identifier
 */
typedef struct _INTERFACE_ID {
    const char *name; 
    const DISC_INTERFACE *interface;
} INTERFACE_ID;

/* All known disc interfaces */
extern const INTERFACE_ID ntfs_disc_interfaces[];

/* Lock volume */
static inline void ntfsLock (ntfs_vd *vd)
{
    LWP_MutexLock(vd->lock);
}

/* Unlock volume */
static inline void ntfsUnlock (ntfs_vd *vd)
{
    LWP_MutexUnlock(vd->lock);
}

/* Miscellaneous helper/support routines */
const char *ntfsRealPath (const char *path);
ntfs_vd *ntfsGetVolume (const char *path);
ntfs_inode *ntfsOpenEntry (ntfs_vd *vd, const char *path);
void ntfsCloseEntry (ntfs_vd *vd, ntfs_inode *ni);
int ntfsCreate (ntfs_vd *vd, const char *path, dev_t type, dev_t dev, const char *target);
int ntfsLink (ntfs_vd *vd, const char *old_path, const char *new_path);
int ntfsUnlink (ntfs_vd *vd, const char *path);
int ntfsStat (ntfs_vd *vd, const char *path, struct stat *st);
void ntfsUpdateTimes (ntfs_vd *vd, ntfs_inode *ni, ntfs_time_update_flags mask);

int ntfsUnicodeToLocal (const ntfschar *ins, const int ins_len, char **outs, int outs_len);
int ntfsLocalToUnicode (const char *ins, ntfschar **outs);

/* Gekko devoptab related routines */
const devoptab_t *ntfsDeviceOpTab (void);
const devoptab_t *ntfsGetDeviceOpTab (const char *name);

#endif /* _NTFSINTERNAL_H */
