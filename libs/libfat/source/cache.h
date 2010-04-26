/*
 cache.h
 The cache is not visible to the user. It should be flushed 
 when any file is closed or changes are made to the filesystem.
 
 This cache implements a least-used-page replacement policy. This will 
 distribute sectors evenly over the pages, so if less than the maximum 
 pages are used at once, they should all eventually remain in the cache.
 This also has the benefit of throwing out old sectors, so as not to keep
 too many stale pages around.

 Copyright (c) 2006 Michael "Chishm" Chisholm
	
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _CACHE_H
#define _CACHE_H

#include "common.h"
#include "disc.h"

#define CACHE_PAGE_SIZE BYTES_PER_READ

typedef struct {
	uint32_t     sector;
	uint8_t      count;
	uint32_t     lastAccess;
	uint8_t*     data;
} RA_CACHE_ENTRY;

typedef struct {
	sec_t        sector;
	unsigned int count;
	bool         dirty;
} CACHE_ENTRY;

typedef struct {
	const DISC_INTERFACE* disc;
	unsigned int          numberOfPages;
	CACHE_ENTRY*          cacheEntries;
	uint8_t*              pages;

	uint32_t              ra_maxSectors;
	uint8_t               ra_count;
	RA_CACHE_ENTRY**      ra_entries;
} CACHE;


/*
Read data from a sector in the cache
If the sector is not in the cache, it will be swapped in
offset is the position to start reading from
size is the amount of data to read
Precondition: offset + size <= BYTES_PER_READ
*/
bool _FAT_cache_readPartialSector (CACHE* cache, void* buffer, sec_t sector, unsigned int offset, size_t size);

bool _FAT_cache_readLittleEndianValue (CACHE* cache, uint32_t *value, sec_t sector, unsigned int offset, int num_bytes);

/*
Write data to a sector in the cache
If the sector is not in the cache, it will be swapped in.
When the sector is swapped out, the data will be written to the disc
offset is the position to start writing to
size is the amount of data to write
Precondition: offset + size <= BYTES_PER_READ
*/
bool _FAT_cache_writePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size);

bool _FAT_cache_writeLittleEndianValue (CACHE* cache, const uint32_t value, sec_t sector, unsigned int offset, int num_bytes);

/*
Write data to a sector in the cache, zeroing the sector first
If the sector is not in the cache, it will be swapped in.
When the sector is swapped out, the data will be written to the disc
offset is the position to start writing to
size is the amount of data to write
Precondition: offset + size <= BYTES_PER_READ
*/
bool _FAT_cache_eraseWritePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size);

/*
Read a full sector from the cache
*/
static inline bool _FAT_cache_readSector (CACHE* cache, void* buffer, sec_t sector) {
	return _FAT_cache_readPartialSector (cache, buffer, sector, 0, BYTES_PER_READ);
}

/*
Write a full sector to the cache
*/
static inline bool _FAT_cache_writeSector (CACHE* cache, const void* buffer, sec_t sector) {
	return _FAT_cache_writePartialSector (cache, buffer, sector, 0, BYTES_PER_READ);
}

/*
Write any dirty sectors back to disc and clear out the contents of the cache
*/
bool _FAT_cache_flush (CACHE* cache);

/* 
Clear out the contents of the cache without writing any dirty sectors first
*/
void _FAT_cache_invalidate (CACHE* cache);

CACHE* _FAT_cache_constructor (unsigned int numberOfPages, const DISC_INTERFACE* discInterface);

void _FAT_cache_destructor (CACHE* cache);

#ifdef LIBFAT_READAHEAD_CACHE
bool _FAT_racache_setParameter(CACHE* cache, const uint8_t numCaches, uint32_t cacheMaxSectors);
void _FAT_racache_destroyByIndex(CACHE* cache, const uint8_t index);
void _FAT_racache_destructor (CACHE* cache);
RA_CACHE_ENTRY* _FAT_racache_getSector (CACHE* cache, const sec_t sector);
#endif

#endif // _CACHE_H

