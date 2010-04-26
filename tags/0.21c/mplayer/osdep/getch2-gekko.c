/*
   getch2_gekko.c - MPlayer TermIO driver for Wii

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

#include "config.h"
#include "keycodes.h"
#include "input/input.h"
#include "mp_fifo.h"

#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

int screen_width = 80;
int screen_height = 24;
char *erase_to_end_of_line = NULL;

static int getch2_status=0;

typedef struct {
	u16 pad;
	u32 wpad;
	int key;
} pad_map;

static const pad_map pad_maps[] = {
	{ PAD_BUTTON_A,		WPAD_BUTTON_A,		'a' },
	{ PAD_BUTTON_B,		WPAD_BUTTON_B,		'b' },
	{ PAD_BUTTON_X,		WPAD_BUTTON_1,		'x' },
	{ PAD_TRIGGER_Z,	WPAD_BUTTON_HOME,	'z' },
	{ PAD_TRIGGER_L,	WPAD_BUTTON_MINUS,	'l' },
	{ PAD_TRIGGER_R,	WPAD_BUTTON_PLUS,	'r' },
	{ PAD_BUTTON_LEFT,	WPAD_BUTTON_LEFT,	KEY_LEFT },
	{ PAD_BUTTON_RIGHT,	WPAD_BUTTON_RIGHT,	KEY_RIGHT },
	{ PAD_BUTTON_UP,	WPAD_BUTTON_UP,		KEY_UP },
	{ PAD_BUTTON_DOWN,	WPAD_BUTTON_DOWN,	KEY_DOWN }
};

static const pad_map pad_maps_mod[] = {
	{ PAD_BUTTON_A,		WPAD_BUTTON_A,		'A' },
	{ PAD_BUTTON_B,		WPAD_BUTTON_B,		'B' },
	{ PAD_BUTTON_X,		WPAD_BUTTON_1,		'X' },
	{ PAD_TRIGGER_Z,	WPAD_BUTTON_HOME,	'Z' },
	{ PAD_TRIGGER_L,	WPAD_BUTTON_MINUS,	'L' },
	{ PAD_TRIGGER_R,	WPAD_BUTTON_PLUS,	'R' },
	{ PAD_BUTTON_LEFT,	WPAD_BUTTON_LEFT,	KEY_KP4 },
	{ PAD_BUTTON_RIGHT,	WPAD_BUTTON_RIGHT,	KEY_KP6 },
	{ PAD_BUTTON_UP,	WPAD_BUTTON_UP,		KEY_KP8 },
	{ PAD_BUTTON_DOWN,	WPAD_BUTTON_DOWN,	KEY_KP2 }
};

void get_screen_size() {
}

void getch2_enable() {
	getch2_status=1;
}

void getch2_disable() {
	getch2_status=0;
}

void getch2(void) {
	static s64 lt = 0;
	s64 tt;
	u16 pad, i;
	u32 wpad;
	bool mod;

	if (!getch2_status)
		return;

	if (reset_pressed || power_pressed) {
		mplayer_put_key(KEY_CLOSE_WIN);
		return;
	}

	tt = gettime();
	if (ticks_to_millisecs(tt - lt) < (TB_MSPERSEC / 60))
		return;

	lt = tt;
	mod = false;

	PAD_ScanPads();

	pad = PAD_ButtonsDown(0);
	if (PAD_ButtonsHeld(0) & PAD_BUTTON_Y)
		mod = true;

	WPAD_ScanPads();

	wpad = 0;
	if (WPAD_Probe (0, NULL) == WPAD_ERR_NONE) {
		wpad = WPAD_ButtonsDown(0);
		if (WPAD_ButtonsHeld(0) & WPAD_BUTTON_2)
			mod = true;
	}

	if (mod) {
		for (i = 0; i < sizeof (pad_maps_mod) / sizeof (pad_map); ++i)
			if ((pad & pad_maps_mod[i].pad) || (wpad & pad_maps_mod[i].wpad)) {
				mplayer_put_key(pad_maps_mod[i].key);
				return;
			}
	} else {
		for (i = 0; i < sizeof (pad_maps) / sizeof (pad_map); ++i)
			if ((pad & pad_maps[i].pad) || (wpad & pad_maps[i].wpad)) {
				mplayer_put_key(pad_maps[i].key);
				return;
			}
	}
}

#ifdef HAVE_ICONV
char* get_term_charset(void) {
	static const char *codepage = "ASCII";

	return codepage;
}
#endif

