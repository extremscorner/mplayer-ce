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

static ao_info_t info = {
	"gekko audio output",
	"gekko",
	"Team Twiizers",
	""
};

const LIBAO_EXTERN(gekko)

#define SFX_BUFFER_SIZE (8*1024)
#define SFX_BUFFERS 32

static mutex_t sfx_mutex = 0;
static u8 buffer[SFX_BUFFERS][SFX_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);
static u8 buffer_fill = 0;
static u8 buffer_play = 0;
static u8 buffer_free = SFX_BUFFERS;
static bool playing = false;

static void switch_buffers() {
	u8 bp;

	AUDIO_StopDMA();

	LWP_MutexLock(sfx_mutex);

	if (playing)
		buffer_free++;

	if (buffer_free == SFX_BUFFERS) {
		playing = false;
		buffer_play = (buffer_play + 1) % SFX_BUFFERS;
		LWP_MutexUnlock(sfx_mutex);
		return;
	}

	bp = buffer_play;
	buffer_play = (buffer_play + 1) % SFX_BUFFERS;
	playing = true;

	LWP_MutexUnlock(sfx_mutex);

	AUDIO_InitDMA((u32) buffer[bp], SFX_BUFFER_SIZE);
	AUDIO_StartDMA();
}

static int control(int cmd, void *arg) {
	//mp_msg(MSGT_AO, MSGL_ERR, "[AOGEKKO]: control %d\n", cmd);

	return CONTROL_UNKNOWN;
}

static int init(int rate, int channels, int format, int flags) {
	//printf("\nAO %s\n", __FUNCTION__);
	LWP_MutexInit(&sfx_mutex, false);

	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(switch_buffers);

	ao_data.buffersize = SFX_BUFFER_SIZE * SFX_BUFFERS;
	ao_data.outburst = SFX_BUFFER_SIZE * 2;
	ao_data.channels = 2;
	ao_data.samplerate = 48000;
	ao_data.format = AF_FORMAT_S16_BE;
	ao_data.bps = 192000;

	return 1;
}

static void reset(void) {
	u8 i;

	//printf("\nAO %s\n", __FUNCTION__);
	AUDIO_StopDMA();

	LWP_MutexLock(sfx_mutex);

	for (i = 0; i < SFX_BUFFERS; ++i) {
		memset(buffer[i], 0, SFX_BUFFER_SIZE);
		DCFlushRange(buffer[i], SFX_BUFFER_SIZE);
	}

	buffer_fill = 0;
	buffer_play = 0;
	buffer_free = SFX_BUFFERS;

	playing = false;

	LWP_MutexUnlock(sfx_mutex);
}

static void uninit(int immed) {
	//printf("\nAO %s\n", __FUNCTION__);
	reset();

	AUDIO_RegisterDMACallback(NULL);

	LWP_MutexDestroy(sfx_mutex);
}

static void audio_pause(void) {
	//printf("\nAO %s\n", __FUNCTION__);
	AUDIO_StopDMA();

	LWP_MutexLock(sfx_mutex);

	if (playing && (buffer_free < SFX_BUFFERS)) {
		buffer_play = (buffer_play + 1) % SFX_BUFFERS;
		buffer_free++;
	}

	playing = false;

	LWP_MutexUnlock(sfx_mutex);
}

static void audio_resume(void) {
	//printf("\nAO %s\n", __FUNCTION__);
	switch_buffers();
}

static int get_space(void) {
	int res;

	LWP_MutexLock(sfx_mutex);

	res = buffer_free * SFX_BUFFER_SIZE;

	LWP_MutexUnlock(sfx_mutex);

	//printf("\nAO %s %d\n", __FUNCTION__, res);
	return res;
}

static int play(void* data, int len, int flags) {
	int bl, ret = 0;
	u8 *s = (u8 *) data;
	bool kick = false;
//return len;
	LWP_MutexLock(sfx_mutex);

	while ((len > 0) && buffer_free) {
		bl = len;
		if (bl > SFX_BUFFER_SIZE)
			bl = SFX_BUFFER_SIZE;

		memcpy(buffer[buffer_fill], s, bl);

		if (bl < SFX_BUFFER_SIZE)
			memset(buffer[buffer_fill] + bl, 0, SFX_BUFFER_SIZE - bl);

		DCFlushRange(buffer[buffer_fill], bl); 

		buffer_fill = (buffer_fill + 1) % SFX_BUFFERS;
		buffer_free--;

		len -= bl;
		s += bl;
		ret += bl;
	}

	if (!playing) 
		kick = true;

	LWP_MutexUnlock(sfx_mutex);

	if (kick)
		switch_buffers();

	//printf("\nAO %s %d\n", __FUNCTION__, ret);
	return ret;
}

static float get_delay(void) {
	float b;

	LWP_MutexLock(sfx_mutex);

	if (buffer_free == SFX_BUFFERS) {
		LWP_MutexUnlock(sfx_mutex);
		return 0;
	}

	b = (SFX_BUFFERS - buffer_free) * SFX_BUFFER_SIZE;

	if (playing)
		b += AUDIO_GetDMABytesLeft();

	LWP_MutexUnlock(sfx_mutex);

	return b / 192000.0f;
}
