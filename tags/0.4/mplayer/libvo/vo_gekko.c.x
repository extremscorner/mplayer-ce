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
static u8 *image_buffer[3] = { NULL, NULL, NULL };

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
						unsigned char *srca, int stride) {
						
	vo_draw_alpha_yv12(w, h, src, srca, stride,
						image_buffer[0] + (y0 * image_width + x0),
						image_width);
						
						
						
}

static u8 *d[3];

static int draw_slice(uint8_t *image[], int stride[], int w, int h, int x,
						int y) {
						
	//mp_msg(MSGT_VO, MSGL_ERR, "draw_slice st[0]:%u  w: %u  h: %u  x: %u  y: %u \n",stride[0],w,h,x,y);
	if(!first_config)
	{ // adjust video width to stride, I don't know how to do better
		first_config = true;
		if(stride[0]!=image_width) config(stride[0], image_height,stride[0], image_height, 0,NULL,0);
		
		GX_ConfigTextureYUV(image_width, image_height, stride);
		pitch[0]=stride[0];	 
		pitch[1]=stride[1];	 
		pitch[2]=stride[2];	 
	}
  
	if(stride[0]!=pitch[0])
	{
	/*
	    //config(stride[0], image_height,stride[0], image_height, 0,NULL,0);
	    //GX_ConfigTextureYUV(image_width, image_height, stride);
  
	    mp_msg(MSGT_VO, MSGL_ERR, "pitch0 %u  stride[0] %u  w: %u  h: %u  image_height: %u\n",pitch[0],stride[0],w,h,image_height);
	    mp_msg(MSGT_VO, MSGL_ERR, "pitch1 %u  stride[1] %u  \n",pitch[1],stride[1]);
	    mp_msg(MSGT_VO, MSGL_ERR, "pitch2 %u  stride[2] %u  \n",pitch[2],stride[2]);
	    log_console_enable_video(true);
		sleep(4);
		log_console_enable_video(false);

	    pitch[0] = stride[0];
    */
//config(stride[0],image_height,stride[0],image_height,0,NULL,0);
//first_config=false;
/*
GX_ConfigTextureYUV(pitch[0], image_height, pitch);

   pitch[0] = stride[0];
	 pitch[1] = stride[1];
	 pitch[2] = stride[2];
     GX_UpdatePitch(stride);
     
	   //GX_ConfigTextureYUV(image_width, image_height, pitch);
   //GX_ConfigTextureYUV(image_width, image_height, pitch);
   */	 
//  
		GX_UpdatePitch(stride[0],image_height,pitch);
	}
    
	if(y==0) GX_ResetTextureYUVPointers(); 
	
	GX_FillTextureYUV(h,image);  
	return 0;	
}

static int draw_slice1(uint8_t *image[], int stride[], int w, int h, int x,
						int y) {
	int i;
	u8 *s[3], *d[3];

	if(!first_config /* || stride[0]!=pitch[0]*/)
	{ // adjust video width to stride, I don't know how to do better
		first_config = true;
		if(stride[0]!=image_width) config(stride[0], image_height,stride[0], image_height, 0,NULL,0);
	}
  
  
	d[0] = image_buffer[0] + y * image_width;// + x;
	d[1] = image_buffer[1] + y * (image_width / 4);// + x / 2;
	d[2] = image_buffer[2] + y * (image_width / 4);// + x / 2;

	if(stride[0]!=pitch[0])
	{
		s[0] = image[0];
		s[1] = image[1];
		s[2] = image[2];
  		for (i = 0; i < h; ++i) 
		{
			memcpy(d[0], s[0], w);
			s[0] += stride[0];
			d[0] += pitch[0];
		}
		for (i = 0; i < h / 2; ++i) 
		{
			memcpy(d[1], s[1], w / 2);
			memcpy(d[2], s[2], w / 2);
			s[1] += stride[1];
			s[2] += stride[2];
			d[1] += pitch[1];
			d[2] += pitch[2];		
		}
	}  
	else 
	{
		memcpy(d[0], image[0], w*h);
		memcpy(d[1], image[1], w*h / 4);
		memcpy(d[2], image[2], w*h / 4);
	}

	return 0;
}

static void draw_osd(void) {
vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page(void) {
	GX_RenderTexture();
	//GX_RenderYUV();
	//sleep(1);
	//GX_RenderYUV(image_width, image_height, image_buffer, pitch);

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

  image_width = ((int)(width/8.0))*8;   
  image_height = ((int)(height/8.0))*8;

  
 	pitch[0] = image_width;
	pitch[1] = image_width / 2;
	pitch[2] = image_width / 2;


  width+=8;   // to be sure we have enough memory
  height+=8;
  if (image_buffer[0]) {
    free(image_buffer[0]);
    free(image_buffer[1]);
    free(image_buffer[2]);
    image_buffer[0] = NULL;
    image_buffer[1] = NULL;
    image_buffer[2] = NULL;
  }

  image_buffer[0] = (u8 *) malloc(width * height);
  image_buffer[1] = (u8 *) malloc(width * height / 4 );
  image_buffer[2] = (u8 *) malloc(width * height / 4 );

	memset(image_buffer[0], 255, width * height);
	memset(image_buffer[1], 255, width * height / 4);
	memset(image_buffer[2], 255, width * height / 4);


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
  
  //mp_msg(MSGT_VO, MSGL_ERR, "[VOGEKKO]: SAR=%0.3f PAR=%0.3f IAR=%0.3f %ux%u -> %ux%u  \n",
  //    sar, par, iar, image_width, image_height, width, height);
  
  //mp_msg(MSGT_VO, MSGL_ERR, "test %u\n",12 >> 3);
  //log_console_enable_video(true);
  //sleep(2);
  //log_console_enable_video(false);

  //GX_SetCamPosZ(350);
  GX_StartYUV(image_width, image_height, width / 2, height / 2 ); 
  
  return 0;
}

static void uninit(void) {

	if (image_buffer[0]) {
		free(image_buffer[0]);
		free(image_buffer[1]);
		free(image_buffer[2]);
		image_buffer[0] = NULL;
		image_buffer[1] = NULL;
		image_buffer[2] = NULL;
	}

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


