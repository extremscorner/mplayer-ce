/*
 racache.c
 
 Copyright (c) 2008 dhewg
	
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

#ifdef LIBFAT_READAHEAD_CACHE

#include <string.h>

#include <ogc/lwp_watchdog.h>

#include "mem_allocate.h"
#include "disc.h"
#include "file_allocation_table.h"
#include "racache.h"


//#define DEBUG_RA

#ifdef DEBUG_RA
#include <stdio.h>
#define pf(...) printf(__VA_ARGS__)
#else
#define pf(...)
#endif


static RA_CACHE_ENTRY* _FAT_racache_constructor(const DISC_INTERFACE* disc, const sec_t sector, const uint8_t numSectors) {
	RA_CACHE_ENTRY* entry = _FAT_mem_allocate(sizeof(RA_CACHE_ENTRY));

	if (entry) {
		entry->data = _FAT_mem_align(numSectors * BYTES_PER_READ);

		if (!entry->data) {
			_FAT_mem_free(entry);
			return NULL;
		}

		if (!_FAT_disc_readSectors(disc, sector, numSectors, entry->data)) {
			_FAT_mem_free(entry->data);
			_FAT_mem_free(entry);
			return NULL;
		}

		entry->sector = sector;
		entry->count = numSectors;
		entry->lastAccess = ticks_to_millisecs(gettime ());
	}
	
	return entry;
}

bool _FAT_racache_readSectors (const PARTITION* partition, const sec_t sector, const sec_t numSectors, void* buffer) {
	RA_CACHE_ENTRY *cache;
	sec_t start, count;
	void* buf = buffer;
	sec_t sec = sector;

	count = numSectors;

	if (partition->cache->ra_entries) {
		cache = _FAT_racache_getSector(partition->cache, sec);

		if (cache) {
			start = sec - cache->sector;
			if (count > cache->count - start)
				count = cache->count - start;

			pf("ra HIT %u (%u) @%u\n", count, start, sec);

			memcpy(buf, cache->data + start * BYTES_PER_READ, count * BYTES_PER_READ);
			buf += count * BYTES_PER_READ;
			sec += count;
			count = numSectors - count;

			cache->lastAccess = ticks_to_millisecs(gettime ());
		}
	}

	if (count == 0)
		return true;

	return _FAT_disc_readSectors(partition->disc, sec, count, buf);
}

bool _FAT_racache_addEntry (const FILE_STRUCT* file) {
	uint32_t i, ra_limit;
	uint32_t ra_start, ra_end, ra_sectors;
	CACHE* cache;
	sec_t sector;

	uint32_t oldest;
	RA_CACHE_ENTRY* entry;

	cache = file->partition->cache;

	if (!cache->ra_entries || file->filesize - file->currentPosition < 1)
		return true;

	ra_limit = (file->filesize - file->currentPosition) / BYTES_PER_READ;

	if (ra_limit < 1)
		ra_limit = 1;

	if (ra_limit > cache->ra_maxSectors)
		ra_limit = cache->ra_maxSectors;

	ra_start = file->rwPosition.cluster;
	ra_sectors = 0;

	while (true) {
		ra_end = ra_start;
		ra_start = _FAT_fat_nextCluster(file->partition, ra_end);

		if (ra_start != ra_end + 1)
			break;

		ra_sectors += file->partition->sectorsPerCluster;

		if (ra_sectors >= ra_limit)
			break;
	}

	if (ra_sectors < 1)
		return true;

	if (ra_sectors > cache->ra_maxSectors)
		ra_sectors = cache->ra_maxSectors;

	sector = _FAT_fat_clusterToSector(file->partition, file->rwPosition.cluster + 1);
	if (_FAT_racache_getSector(cache, sector)) {
		pf("ra %u IS CACHED\n", sector);
		return true;
	}

	oldest = 0;
	entry = NULL;

	for (i = 0; i < cache->ra_count; ++i) {
		if (!cache->ra_entries[i]) {
			pf("ra: new #%u - %u @%u\n", i, ra_sectors, sector);			
			entry = _FAT_racache_constructor(file->partition->disc, sector, ra_sectors);
			cache->ra_entries[i] = entry;
			if(entry==NULL)
			{
				entry = _FAT_racache_constructor(file->partition->disc, sector, ra_sectors);
			}
			break;
		} else {
			if (cache->ra_entries[i]->lastAccess < cache->ra_entries[oldest]->lastAccess)
				oldest = i;
		}
	}

	if (!entry) {
		
		pf("ra: reuse #%u - %u @%u\n", oldest, ra_sectors, sector);		
		_FAT_racache_destroyByIndex(cache, oldest);
		entry = _FAT_racache_constructor(file->partition->disc, sector, ra_sectors);
		cache->ra_entries[oldest] = entry;
	} 
	return entry != NULL;
}
#endif

