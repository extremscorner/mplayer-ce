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

#include "libavcodec/fft.h"
#include "libavutil/ppc/paired.h"

static void ff_rdft_calc_paired(RDFTContext *s, FFTSample *data)
{
	int i, i1, i2;
	vector float pair, even, odd;
	const int n = 1 << s->nbits;
	const float k1 = 0.5;
	vector float k2 = {0.5-s->inverse,-(0.5-s->inverse)};
	const FFTSample *tcos = s->tcos;
	const FFTSample *tsin = s->tsin;
	vector float d1, d2, ng, mr;
	
	if (!s->inverse) {
		ff_fft_permute(&s->fft, (FFTComplex *)data);
		ff_fft_calc(&s->fft, (FFTComplex *)data);
	}
	
	pair = psq_l(0,data,0,0);
	even = paired_sum0(pair, pair, pair);
	odd = paired_neg(pair);
	pair = paired_sum1(pair, even, odd);
	psq_st(pair,0,data,0,0);
	
	for (i = 1; i < (n>>2); i++) {
		i1 = 8*i;
		i2 = 4*n-i1;
		
		d1 = psq_lx(i1,data,0,0);
		d2 = psq_lx(i2,data,0,0);
		
		ng = paired_neg(d2);
		mr = paired_merge01(d2, ng);
		even = paired_add(d1, mr);
		even = ps_muls0(even, k1);
		
		d1 = paired_merge10(d1, d1);
		mr = paired_merge10(d2, ng);
		odd = paired_add(d1, mr);
		odd = paired_mul(odd, k2);
		
		float vcos = tcos[i];
		float vsin = tsin[i];
		
		mr = paired_merge10(odd, odd);
		ng = paired_neg(mr);
		ng = paired_merge01(ng, mr);
		
		pair = ps_madds0(ng, vsin, even);
		pair = ps_madds0(odd, vcos, pair);
		psq_stx(pair,i1,data,0,0);
		
		pair = ps_muls0(odd, vcos);
		d1 = paired_merge01(even, pair);
		d2 = paired_merge01(pair, even);
		pair = paired_sub(d1, d2);
		pair = ps_madds0(mr, vsin, pair);
		psq_stx(pair,i2,data,0,0);
	}
	
	data[2*i+1] = s->sign_convention * data[2*i+1];
	
	if (s->inverse) {
		pair = psq_l(0,data,0,0);
		pair = ps_muls0(pair, k1);
		psq_st(pair,0,data,0,0);
		
		ff_fft_permute(&s->fft, (FFTComplex *)data);
		ff_fft_calc(&s->fft, (FFTComplex *)data);
	}
}

av_cold void ff_rdft_init_paired(RDFTContext *s)
{
	s->rdft_calc = ff_rdft_calc_paired;
}
