/*
   ao_gekko.c - MPlayer audio driver for Wii

   Copyright (C) 2008 dhewg

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

#include "config.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "help_mp.h"

#include <ogcsys.h>
#include "osdep/plat_gekko.h"

static ao_info_t info = {
	"gekko audio output",
	"gekko",
	"Team Twiizers",
	""
};

LIBAO_EXTERN(gekko)

#define SFX_BUFFER_SIZE 4096
#define SFX_BUFFERS 32

static u8 buffer[SFX_BUFFERS][SFX_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);
static int size[SFX_BUFFERS];
static u8 buffer_fill = 0;
static u8 buffer_play = 0;
static u8 buffer_free = SFX_BUFFERS;
static bool playing = false;
static unsigned int bytes_buffered;

static void switch_buffers() {
	AUDIO_StopDMA();

	if (playing)
		buffer_free++;

	if (buffer_free == SFX_BUFFERS) {
		playing = false;
		return;
	}


	AUDIO_InitDMA((u32) buffer[buffer_play], size[buffer_play]);
	AUDIO_StartDMA();

	size[buffer_play]=0;
	buffer_play = (buffer_play + 1) % SFX_BUFFERS;

	playing = true;
}

static int control(int cmd, void *arg) {
	//mp_msg(MSGT_AO, MSGL_ERR, "[AOGEKKO]: control %d\n", cmd);
	return CONTROL_UNKNOWN;
}

void reinit_audio()  // for newgui
{
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(switch_buffers);

	ao_data.buffersize = SFX_BUFFER_SIZE * SFX_BUFFERS;
	ao_data.outburst = SFX_BUFFER_SIZE;
	ao_data.channels = 2;
	ao_data.samplerate = 48000;
	ao_data.format = AF_FORMAT_S16_BE;
	ao_data.bps = 192000;
}

static int init(int rate, int channels, int format, int flags) {
	u8 i;

	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(switch_buffers);

	ao_data.buffersize = SFX_BUFFER_SIZE * SFX_BUFFERS;
	ao_data.outburst = SFX_BUFFER_SIZE;
	ao_data.channels = 2;
	ao_data.samplerate = 48000;
	ao_data.format = AF_FORMAT_S16_BE;
	ao_data.bps = 192000;

	for (i = 0; i < SFX_BUFFERS; ++i) size[i]=0;
	
	return 1;
}

static void reset(void) {
	u8 i;

	AUDIO_StopDMA();
	for (i = 0; i < SFX_BUFFERS; ++i) {
		memset(buffer[i], 0, SFX_BUFFER_SIZE);
		DCFlushRange(buffer[i], SFX_BUFFER_SIZE);
		size[i]=0;
	}
	
	buffer_fill = 0;
	buffer_play = 0;
	buffer_free = SFX_BUFFERS;

	playing = false;
	
	//to be sure dma is clean	
	AUDIO_InitDMA((u32)buffer[0],0);
	AUDIO_StartDMA();
	usleep(50);
	while(AUDIO_GetDMABytesLeft()>0) usleep(50);
	AUDIO_StopDMA();
	
}

static void uninit(int immed) {
	reset();

	AUDIO_RegisterDMACallback(NULL);
}

static void audio_pause(void) {
	AUDIO_StopDMA();

	if (playing && (buffer_free < SFX_BUFFERS)) {
		buffer_play = (buffer_play + 1) % SFX_BUFFERS;
		buffer_free++;
	}

	playing = false;
}

static void audio_resume(void) {
	switch_buffers();
}

static int get_space(void) {
	return (buffer_free - 1) * SFX_BUFFER_SIZE;
}

#define SWAP(x) ((x>>16)|(x<<16))
static void copy_swap_channels(u32 *d, u32 *s, int len)
{
	int n;
	
	len=len/4;
	
	for(n=0;n<len;n++) d[n] = SWAP(s[n]);
}
static int play(void* data, int len, int flags) {
	int bl, ret = 0;
	u8 v;
	u8 *s = (u8 *) data;

	while ((len > 0) && (buffer_free > 1)) {
		bl = len;
		if (bl > SFX_BUFFER_SIZE)
			bl = SFX_BUFFER_SIZE;

		size[buffer_fill]=bl;
		
		copy_swap_channels(buffer[buffer_fill],s,bl);
		//memcpy(buffer[buffer_fill], s, bl);
		DCFlushRange(buffer[buffer_fill], bl);

		buffer_fill = (buffer_fill + 1) % SFX_BUFFERS;
		buffer_free--;

		len -= bl;
		s += bl;
		ret += bl;
	}

	if (!playing)
		switch_buffers();

	return ret;
}

static float get_delay(void) {
	float b=0;

	if (buffer_free == SFX_BUFFERS)
		return 0;

	b = (SFX_BUFFERS - buffer_free - 1) * SFX_BUFFER_SIZE;

	if (playing)
		b += AUDIO_GetDMABytesLeft();

	return b / 192000.0f;
}

