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

static vo_info_t info = {
	"gekko video output",
	"gekko",
	"Team Twiizers",
	""
};

LIBVO_EXTERN(gekko)

/*
static int cam_pos_z = 350;

static opt_t subopts[] = {
	{ "cam_pos_z", OPT_ARG_INT,  &cam_pos_z, (opt_test_f) int_non_neg },
	{ NULL }
};
*/

static	u16 pitch[3];

static u8 *image_buffer[3] = { NULL, NULL, NULL };
static u32 image_width = 0, image_height = 0;

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
						unsigned char *srca, int stride) {
	vo_draw_alpha_yv12(w, h, src, srca, stride,
						image_buffer[0] + (y0 * image_width + x0),
						image_width);
}

static int draw_slice(uint8_t *image[], int stride[], int w, int h, int x,
						int y) {
	int i;
	u8 *s[3], *d[3];

  w=image_width;
	s[0] = image[0];
	s[1] = image[1];
	s[2] = image[2];
	d[0] = image_buffer[0] + y * image_width + x;
	d[1] = image_buffer[1] + y * image_width / 4 + x / 2;
	d[2] = image_buffer[2] + y * image_width / 4 + x / 2;

	for (i = 0; i < h; ++i) {
		memcpy(d[0], s[0], w);
		s[0] += stride[0];
		d[0] += image_width;
	}

	for (i = 0; i < h / 2; ++i) {
		memcpy(d[1], s[1], w / 2);
		memcpy(d[2], s[2], w / 2);
		s[1] += stride[1];
		s[2] += stride[2];
		d[1] += image_width / 2;
		d[2] += image_width / 2;
		
	}
//mp_msg(MSGT_VO, MSGL_ERR, "[draw_slice]: w=%u  h=%u  x=%u  y=%u  iw=%u ih=%u \n",w,h,x,y,image_width,image_height);
//sleep(1);
	return 0;
}

static void draw_osd(void) {
	vo_draw_text(image_width, image_height, draw_alpha);
}

static void inline flip_page(void) {

	GX_RenderYUV(image_width, image_height, image_buffer, pitch);
	
}

static int inline draw_frame(uint8_t *src[]) {
	//mp_msg(MSGT_VO, MSGL_ERR, "[VOGEKKO]: draw_frame\n");

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

  uint32_t orig_width,orig_height;

  orig_width=width;
  orig_height=height;

  if(height%8!=0)
  {
    height = height/8.0;    
    //if(height%2!=0)height++;
    height=height*8;
  }
  
  if(width%8!=0)
  {
    width = width/8.0;    
    if(width%2!=0)width++;
    width=width*8;
  }

  image_width = width;
  image_height = height;

  width=orig_width+16;   // to be sure we have enough memory
  height=orig_height+16;
  
  if (image_buffer[0]) {
    free(image_buffer[0]);
    free(image_buffer[1]);
    free(image_buffer[2]);
    image_buffer[0] = NULL;
    image_buffer[1] = NULL;
    image_buffer[2] = NULL;
  }

  image_buffer[0] = (u8 *) malloc(width * height);
  image_buffer[1] = (u8 *) malloc(width * height / 4);
  image_buffer[2] = (u8 *) malloc(width * height / 4);

	memset(image_buffer[0], 0, width * height);
	memset(image_buffer[1], 0, width * height / 4);
	memset(image_buffer[2], 0, width * height / 4);

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

  
  mp_msg(MSGT_VO, MSGL_ERR, "[VOGEKKO]: SAR=%0.3f PAR=%0.3f IAR=%0.3f %ux%u -> %ux%u  vh:%u\n",
      sar, par, iar, image_width, image_height, width, height,vmode->viHeight);
  
  //log_console_enable_video(true);
  //sleep(3);
  //log_console_enable_video(false);

	pitch[0] = image_width;
	pitch[1] = image_width / 2;
	pitch[2] = image_width / 2;

  GX_StartYUV(image_width, image_height, width / 2, height / 2);
  

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
/*
	if (subopt_parse(arg, subopts) != 0)
		mp_msg(MSGT_VO, MSGL_ERR, "[VOGEKKO]: ignoring unknown options: %s\n",
				arg);

	mp_msg(MSGT_VO, MSGL_ERR, "[VOGEKKO]: cam_pos_z=%d\n", cam_pos_z);

	GX_SetCamPosZ(cam_pos_z);
*/
  //GX_SetCamPosZ(350);
	log_console_enable_video(false);

	return 0;
}

static int control(uint32_t request, void *data, ...) {
	switch (request) {
	case VOCTRL_QUERY_FORMAT:
		return query_format(*((uint32_t*) data));
	}

	return VO_NOTIMPL;
}
