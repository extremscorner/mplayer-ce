/*
   vo_gekko.c - MPlayer video driver for Wii

   Copyright (C) 2008 dhewg

   sepp256 - Added YUV frame rendering functions.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>

#include "config.h"
#include "mp_msg.h"
#include "subopt-helper.h"
#include "help_mp.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mp_fifo.h"
#include "osd.h"
#include "sub.h"
#include "osdep/keycodes.h"
#include "osdep/log_console.h"
#include "osdep/gx_supp.h"
#include "fastmemcpy.h"

#include <gccore.h>

static const vo_info_t info = {
	"gekko video output",
	"gekko",
	"Team Twiizers",
	""
};

const LIBVO_EXTERN (gekko)
static bool first_config=false;
static	u16 pitch[3];
static u32 image_width = 0, image_height = 0;

void vo_draw_alpha_gekko(int w,int h, unsigned char* src, unsigned char *srca, 
	int srcstride, unsigned char* dstbase,int dststride)
{
// can be optimized
    int y;
 	unsigned char* buf,*bufa, *tmp,*tmpa;
	int buf_st;
	int h1,w1,Yrowpitch,df1;
	
	u8 *dst, 
			*srca1,*src1,
			*srca2,*src2,
			*srca3,*src3,
			*srca4,*src4;


	h1 = ((h/8.0)+0.5)*8;
	buf = malloc(dststride * h1);
	bufa = malloc(dststride * h1);

	memset(buf, 0, dststride * h1);
	memset(bufa, 0, dststride * h1);

	buf_st=(dststride-srcstride)/2;
	tmp=buf+buf_st;
	tmpa=bufa+buf_st;
    
    for(y=0;y<h;y++){
    	memcpy(tmp, src, w);
    	memcpy(tmpa, srca, w);
        src+=srcstride;
        srca+=srcstride;
        tmp+=dststride;
        tmpa+=dststride;        
    }	
	w=srcstride=dststride;
	h=h1;
	
	src=buf;
	srca=bufa;
	h1 = h / 4 ;

	if(dststride>image_width)w1 = image_width >> 3 ;
	else w1 = dststride >> 3 ;
    Yrowpitch=GetYrowpitch()*8;
    df1 = ((image_width >> 3) - w1)*32;    

				
	dst=dstbase;
    srca1=srca;
    src1=src;
    srca2=srca+ dststride;
    src2=src+ dststride;
    srca3=srca+ dststride*2;
    src3=src+ dststride*2;
    srca4=srca+ dststride*3;
    src4=src+ dststride*3;
    int x;
	for (y = 0; y < h1; y++) {
		for (w = 0; w < w1; w++) {
			for(x=0;x<8;x++)
			{
				if(*srca1) *dst =(((*dst)*(*srca1))>>8)+(*src1);
				dst++;srca1++;src1++;
			}
			for(x=0;x<8;x++)
			{
				if(*srca2) *dst =(((*dst)*(*srca2))>>8)+(*src2);
				dst++;srca2++;src2++;
			}
			for(x=0;x<8;x++)
			{
				if(*srca3) *dst=(((*dst)*(*srca3))>>8)+(*src3);
				dst++;srca3++;src3++;
			}
			for(x=0;x<8;x++)
			{
				if(*srca4) *dst=(((*dst)*(*srca4))>>8)+(*src4);
				dst++;srca4++;src4++;
			}			
		}
		dst+=df1;
		srca1 += Yrowpitch;
		src1 += Yrowpitch;
		srca2 += Yrowpitch;
		src2 += Yrowpitch;
		srca3 += Yrowpitch;
		src3 += Yrowpitch;
		srca4 += Yrowpitch;
		src4 += Yrowpitch;
	}
	free(buf);
	free(bufa);
   
}


static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
						unsigned char *srca, int stride) {
					
	y0=((int)(y0/8.0))*8;				
	vo_draw_alpha_gekko(w, h, src, srca, stride,
						GetYtexture() + (y0 * image_width),
						pitch[0]);

}

static int draw_slice(uint8_t *image[], int stride[], int w, int h, int x,
						int y) {
	if(y==0) 
	{
		GX_ResetTextureYUVPointers();
		if(stride[0]!=pitch[0])
		{
			pitch[0]=stride[0];
			pitch[1]=stride[1];
			pitch[2]=stride[2];
			GX_UpdatePitch(image_width,pitch);
		}
		
	} 
	GX_FillTextureYUV(h,image,stride);  
	return 0;	
}

static void draw_osd(void) {
vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page(void) {
	GX_RenderTexture();
}

static int draw_frame(uint8_t *src[]) {
	return 0;
}

static int inline query_format(uint32_t format) {

	switch (format) {
	case IMGFMT_YV12:
		return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
				VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_ACCEPT_STRIDE;
	default:
		return 0;
	}
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
          uint32_t d_height, uint32_t flags, char *title,
          uint32_t format) {
	float sar, par, iar;
	image_width = (width / 16);
	if(image_width % 2) image_width++;
	image_width=image_width*16;
	image_height = ((int)((height/8.0)))*8;
	
 	pitch[0] = image_width;
	pitch[1] = image_width / 2;
	pitch[2] = image_width / 2;


  if (CONF_GetAspectRatio())
    sar = 16.0f / 9.0f;
  else
    sar = 4.0f / 3.0f;

  iar = (float) d_width / (float) d_height;  
  par = (float) d_width / (float) d_height;
  par *= (float) vmode->fbWidth / (float) vmode->xfbHeight;
  par /= sar;

  if (iar > sar) {
    width = vmode->viWidth;
    height = (float) width / par;
  } else {
    height = vmode->viHeight;
    width = (float) height * par + vmode->viWidth - vmode->fbWidth;
  }
  
  GX_StartYUV(image_width, image_height, width / 2, height / 2 ); 
  GX_ConfigTextureYUV(image_width, image_height, pitch);	
  return 0;
}

static void uninit(void) {
	image_width = 0;
	image_height = 0;
}

static void check_events(void) {
}

static int preinit(const char *arg) {
	log_console_enable_video(false);
	first_config=false;
	
	return 0;
}

static int control(uint32_t request, void *data, ...) {
	switch (request) {
	case VOCTRL_QUERY_FORMAT:
		return query_format(*((uint32_t*) data));
	}

	return VO_NOTIMPL;
}


