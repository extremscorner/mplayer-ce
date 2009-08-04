/**
 * mem_allocate.h - Functions for dealing with conversion of data between types.
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

#ifndef _BIT_OPS_H
#define _BIT_OPS_H

#include <stdint.h>

/*-----------------------------------------------------------------
Functions to deal with little endian values stored in uint8_t arrays
-----------------------------------------------------------------*/
static inline uint16_t u8array_to_u16 (const uint8_t* item, int offset) {
    return ( item[offset] | (item[offset + 1] << 8));
}

static inline uint32_t u8array_to_u32 (const uint8_t* item, int offset) {
    return ( item[offset] | (item[offset + 1] << 8) | (item[offset + 2] << 16) | (item[offset + 3] << 24));
}

static inline void u16_to_u8array (uint8_t* item, int offset, uint16_t value) {
    item[offset]     = (uint8_t) value;
    item[offset + 1] = (uint8_t)(value >> 8);
}

static inline void u32_to_u8array (uint8_t* item, int offset, uint32_t value) {
    item[offset]     = (uint8_t) value;
    item[offset + 1] = (uint8_t)(value >> 8);
    item[offset + 2] = (uint8_t)(value >> 16);
    item[offset + 3] = (uint8_t)(value >> 24);
}

#endif /* _BIT_OPS_H */
