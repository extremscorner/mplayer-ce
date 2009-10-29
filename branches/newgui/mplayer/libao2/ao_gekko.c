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

#define SFX_BUFFER_SIZE (4*1024)
#define SFX_BUFFERS 32

static u8 buffer[SFX_BUFFERS][SFX_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);
static u8 buffer_fill = 0;
static u8 buffer_play = 0;
static bool playing = false;
static unsigned int bytes_buffered = 0;

static void switch_buffers()
{
	if(!playing || bytes_buffered == 0)
		return;

	AUDIO_StopDMA();

	DCFlushRange(buffer[buffer_play], SFX_BUFFER_SIZE);
	AUDIO_InitDMA((u32)buffer[buffer_play], SFX_BUFFER_SIZE);
	AUDIO_StartDMA();
	buffer_play = (buffer_play + 1) % SFX_BUFFERS;
	bytes_buffered -= SFX_BUFFER_SIZE;
}

static int control(int cmd, void *arg)
{
	//mp_msg(MSGT_AO, MSGL_ERR, "[AOGEKKO]: control %d\n", cmd);
	return CONTROL_UNKNOWN;
}

void reinit_audio() // for newgui
{
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(switch_buffers);
}

static int init(int rate, int channels, int format, int flags)
{
	u8 i;

	ao_data.buffersize = SFX_BUFFER_SIZE * SFX_BUFFERS;
	ao_data.outburst = SFX_BUFFER_SIZE;
	ao_data.channels = 2;
	ao_data.samplerate = 48000;
	ao_data.format = AF_FORMAT_S16_BE;
	ao_data.bps = 192000;

	for (i = 0; i < SFX_BUFFERS; ++i)
		memset(buffer[i], 0, SFX_BUFFER_SIZE);

	buffer_fill = 0;
	buffer_play = 0;
	bytes_buffered = 0;
	playing = false;

	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(switch_buffers);

	return 1;
}

static void reset(void)
{
	u8 i;

	AUDIO_StopDMA();

	for (i = 0; i < SFX_BUFFERS; ++i)
		memset(buffer[i], 0, SFX_BUFFER_SIZE);

	buffer_fill = 0;
	buffer_play = 0;
	bytes_buffered = 0;
	playing = false;

	 // flush DMA
	 DCFlushRange(buffer[0], SFX_BUFFER_SIZE);
	 AUDIO_InitDMA((u32)buffer[0],SFX_BUFFER_SIZE);
	 AUDIO_StartDMA();
	 usleep(50);
	 while(AUDIO_GetDMABytesLeft()>0) usleep(50);
	 AUDIO_StopDMA();
}

static void uninit(int immed)
{
	reset();
	AUDIO_RegisterDMACallback(NULL);
}

static void audio_pause(void)
{
	AUDIO_StopDMA();
	playing = false;
}

static void audio_resume(void)
{

}

static int get_space(void)
{
	return SFX_BUFFERS * SFX_BUFFER_SIZE - bytes_buffered;
}

#define SWAP(x) ((x>>16)|(x<<16))

static void copy_swap_channels(u32 *d, u32 *s)
{
	int n;

	for(n = 0; n < 1024; n++) // SFX_BUFFER_SIZE / 4
		d[n] = SWAP(s[n]);
}

static int play(void* data, int len, int flags)
{
	u8 *s = (u8 *)data;
	int bl = 0;

	while (bl < len && get_space() > SFX_BUFFER_SIZE)
	{
		if(len - bl < SFX_BUFFER_SIZE)
			break;

		copy_swap_channels((u32*)buffer[buffer_fill], (u32*)s);
		buffer_fill = (buffer_fill + 1) % SFX_BUFFERS;
		bytes_buffered += SFX_BUFFER_SIZE;
		bl += SFX_BUFFER_SIZE;
		s += SFX_BUFFER_SIZE;

		if (!playing)
		{
			playing = true;
			switch_buffers();
		}
	}
	return bl;
}

static float get_delay(void)
{
	float b = (float) bytes_buffered;

	if (playing)
		b += AUDIO_GetDMABytesLeft();

	return b / 192000.0f;
}

