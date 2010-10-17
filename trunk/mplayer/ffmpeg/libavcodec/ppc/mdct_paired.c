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

void ff_imdct_half_paired(FFTContext *s, FFTSample *output, const FFTSample *input)
{
	int n = 1 << s->mdct_bits;
	int n2 = n >> 1;
	int n4 = n >> 2;
	int n8 = n >> 3;
	
	const uint16_t *revtab = s->revtab;
	const FFTSample *tcos = s->tcos;
	const FFTSample *tsin = s->tsin;
	FFTComplex *z = (FFTComplex *)output;
	
	vector float pair[2], sub[2], add[2];
	vector float result, cos, sin;
	
	FFTSample *asbase[2][2] = {{input-2,input+n2+1},{tcos-2,tsin-2}};
	
	for (int k=0; k<n4; k+=2) {
		add[0] = psq_lu(8,asbase[0][0],1,0);
		add[1] = psq_lu(8,asbase[0][0],1,0);
		pair[0] = ps_merge00(add[0], add[1]);
		
		sub[0] = psq_lu(-8,asbase[0][1],1,0);
		sub[1] = psq_lu(-8,asbase[0][1],1,0);
		pair[1] = ps_merge00(sub[0], sub[1]);
		
		cos = psq_lu(8,asbase[1][0],0,0);
		sin = psq_lu(8,asbase[1][1],0,0);
		
		sub[0] = paired_mul(pair[0], sin);
		sub[0] = paired_msub(pair[1], cos, sub[0]);
		
		add[0] = paired_mul(pair[0], cos);
		add[0] = paired_madd(pair[1], sin, add[0]);
		
		result = paired_merge00(sub[0], add[0]);
		psq_stx(result,revtab[k]*8,z,0,0);
		result = paired_merge11(sub[0], add[0]);
		psq_stx(result,revtab[k+1]*8,z,0,0);
	}
	
	ff_fft_calc(s, z);
	
	FFTComplex *cbase[2] = {z+n8,z+n8-1};
	FFTSample *bsbase[2][2] = {{tcos+n8,tcos+n8-2},{tsin+n8,tsin+n8-2}};
	
	for (int k=0; k<n8; k+=2) {
		pair[0] = psq_lu(-8,cbase[0],0,0);
		pair[1] = psq_l(-8,cbase[0],0,0);
		
		cos = psq_lu(-8,bsbase[0][0],0,0);
		sin = psq_lu(-8,bsbase[1][0],0,0);
		
		result = paired_merge00(pair[1], pair[0]);
		sub[0] = paired_mul(result, cos);
		add[1] = paired_mul(result, sin);
		result = paired_merge11(pair[1], pair[0]);
		sub[0] = paired_msub(result, sin, sub[0]);
		add[1] = paired_madd(result, cos, add[1]);
		
		pair[0] = psq_lu(8,cbase[1],0,0);
		pair[1] = psq_l(8,cbase[1],0,0);
		
		cos = psq_lu(8,bsbase[0][1],0,0);
		sin = psq_lu(8,bsbase[1][1],0,0);
		
		result = paired_merge00(pair[0], pair[1]);
		sub[1] = paired_mul(result, cos);
		add[0] = paired_mul(result, sin);
		result = paired_merge11(pair[0], pair[1]);
		sub[1] = paired_msub(result, sin, sub[1]);
		add[0] = paired_madd(result, cos, add[0]);
		
		result = paired_merge10(sub[0], add[0]);
		psq_st(result,0,cbase[0],0,0);
		result = paired_merge01(sub[0], add[0]);
		psq_stu(result,-8,cbase[0],0,0);
		
		result = paired_merge01(sub[1], add[1]);
		psq_st(result,0,cbase[1],0,0);
		result = paired_merge10(sub[1], add[1]);
		psq_stu(result,8,cbase[1],0,0);
	}
}

void ff_imdct_calc_paired(FFTContext *s, FFTSample *output, const FFTSample *input)
{
	int n = 1 << s->mdct_bits;
	int n2 = n >> 1;
	int n4 = n >> 2;
	
	ff_imdct_half_paired(s, output+n4, input);
	
	vector float pair;
	FFTSample *base[2][2] = {{output+n2,output-2},{output+n2-2,output+n}};
	
	for (int k=0; k<n4; k+=2) {
		pair = psq_lu(-8,base[0][0],0,0);
		pair = paired_neg(pair);
		pair = paired_merge10(pair, pair);
		psq_stu(pair,8,base[0][1],0,0);
		
		pair = psq_lu(8,base[1][0],0,0);
		pair = paired_merge10(pair, pair);
		psq_stu(pair,-8,base[1][1],0,0);
	}
}
