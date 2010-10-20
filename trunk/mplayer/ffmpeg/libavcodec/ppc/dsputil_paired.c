/*
 * This file is part of MPlayer CE.
 *
 * MPlayer CE is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * MPlayer CE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with MPlayer CE; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavcodec/dsputil.h"
#include "dsputil_paired.h"
#include "libavutil/ppc/paired.h"

static void vorbis_inverse_coupling_paired(float *mag, float *ang, int blocksize)
{
	const vector float zero = {0.0,0.0};
	
	vector float pair[2];
	vector float neg, sel;
	
	for (int i=0; i<blocksize*4-7; i+=8) {
		pair[0] = paired_lx(i, mag);
		pair[1] = paired_lx(i, ang);
		
		neg = paired_neg(pair[1]);
		sel = paired_sel(pair[0], pair[1], neg);
		neg = paired_neg(sel);
		sel = paired_sel(pair[1], neg, sel);
		sel = paired_add(pair[0], sel);
		
		if (paired_cmpu0(LT, pair[1], zero)) {
			pair[1] = paired_merge01(pair[0], pair[1]);
			pair[0] = paired_merge01(sel, pair[0]);
		} else
			pair[1] = paired_merge01(sel, pair[1]);
		
		if (paired_cmpu1(LT, pair[1], zero)) {
			pair[1] = paired_merge01(pair[1], pair[0]);
			pair[0] = paired_merge01(pair[0], sel);
		} else
			pair[1] = paired_merge01(pair[1], sel);
		
		paired_stx(pair[0], i, mag);
		paired_stx(pair[1], i, ang);
	}
}

void dsputil_init_paired(DSPContext *c, AVCodecContext *avctx)
{
	if (CONFIG_VORBIS_DECODER)
		c->vorbis_inverse_coupling = vorbis_inverse_coupling_paired;
}
