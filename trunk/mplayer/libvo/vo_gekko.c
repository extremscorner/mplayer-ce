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
#include "sub/osd.h"
#include "sub/sub.h"
#include "aspect.h"
#include "osdep/gx_supp.h"

static const vo_info_t info = {
	"gekko video output",
	"gekko",
	"Team Twiizers",
	""
};

const LIBVO_EXTERN (gekko)

static u32 image_width = 0, image_height = 0;

extern int screenwidth;
extern int screenheight;

static void resize(void)
{
	aspect(&vo_dwidth, &vo_dheight, vo_fs ? A_ZOOM : A_NOZOOM);
	panscan_calc();
	
	vo_dwidth += vo_panscan_x;
	vo_dheight += vo_panscan_y;
	
	mpgxSetSquare((f32)vo_dwidth / 2, (f32)vo_dheight / 2);
}

static int draw_slice(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	mpgxIsDrawDone();
	mpgxCopyYUVp(image, stride);
	return VO_FALSE;
}

static void draw_osd(void)
{
	mpgxIsDrawDone();
	vo_draw_text(image_width, image_height, mpgxBlitOSD);
}

static void flip_page(void)
{
	mpgxIsDrawDone();
	mpgxPushFrame();
}

static uint32_t draw_image(mp_image_t *mpi)
{
	if (mpi->flags & MP_IMGFLAG_PLANAR) {
		mpgxIsDrawDone();
		mpgxCopyYUVp(mpi->planes, mpi->stride);
	}
	
	return VO_TRUE;
}

static int draw_frame(uint8_t *src[])
{
	return VO_ERROR;
}

static int query_format(uint32_t format)
{
	if (mp_get_chroma_shift(format, NULL, NULL))	// Accept any planar YUV format.
		return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_ACCEPT_STRIDE | VOCAP_NOSLICES;
	else return VO_FALSE;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	image_width = width;
	image_height = height;
	
	vo_fs = flags & VOFLAG_FULLSCREEN;
	
	vo_dwidth = d_width;
	vo_dheight = d_height;
	
	vo_screenwidth = screenwidth;
	vo_screenheight = screenheight;
	
	aspect_save_orig(width, height);
	aspect_save_prescale(d_width, d_height);
	aspect_save_screenres(vo_screenwidth, vo_screenheight);
	
	resize();
	
	int xs, ys;
	mp_get_chroma_shift(format, &xs, &ys);
	
	mpgxConfigYUVp(image_width, image_height, image_width >> xs, image_height >> ys);
	return VO_FALSE;
}

static void uninit(void)
{
	image_width = 0;
	image_height = 0;
}

static void check_events(void)
{
}

static int preinit(const char *arg)
{
	mpgxInit();
	mpgxSetupYUVp();
	return VO_FALSE;
}

static int control(uint32_t request, void *data, ...)
{
	switch (request) {
		case VOCTRL_QUERY_FORMAT:
			return query_format(*((uint32_t *)data));
		case VOCTRL_DRAW_IMAGE:
			return draw_image(data);
		case VOCTRL_FULLSCREEN:
			vo_fs = !vo_fs;
			resize();
			return VO_TRUE;
		case VOCTRL_GET_PANSCAN:
			return VO_TRUE;
		case VOCTRL_SET_PANSCAN:
			resize();
			return VO_TRUE;
		default:
			return VO_NOTIMPL;
	}
}
