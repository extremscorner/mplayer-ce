/*
   af_wiilight.c - Basic visualization implemented as audio filter

   Copyright (C) 2010 Extrems

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
#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "osdep/wiilight.h"
#include "af.h"


static int control(struct af_instance_s *af, int cmd, void *arg)
{
	switch(cmd)
	{
		case AF_CONTROL_REINIT:
		{
			if (!arg)
				return AF_ERROR;
			
			af_data_t *data = (af_data_t*)arg;
			
			// (from ao_asnd)
			int channels = data->nch;
			
			if (channels > 2)
				channels = 2;	// Surround would drag the average down.
			
			int format = data->format;
			int bits = format & AF_FORMAT_BITS_MASK;
			int bytes = 2;
			
			if (bits > AF_FORMAT_8BIT) { format = AF_FORMAT_S16_NE; }
			else { format = AF_FORMAT_S8; bytes = 1; }
			
			af->data->rate = data->rate;
			af->data->nch = channels;
			af->data->format = format;
			af->data->bps = bytes;
			
			return af_test_output(af, data);
		}
		default:
			return AF_UNKNOWN;
	}
}

static void uninit(struct af_instance_s *af)
{
	if (af->data)
		free(af->data);
	
	WIILIGHT_TurnOff();
}

static af_data_t *play(struct af_instance_s *af, af_data_t *data)
{
	double average = 0.0;
	int samples = data->len / af->data->bps;
	
	for (int counter = 0; counter < samples; counter++)
	{
		double value = 0.0;
		
		switch(af->data->format)
		{
			case AF_FORMAT_S16_NE:
				value = (double)(((short *)data->audio)[counter]) / SHRT_MAX;
				break;
			case AF_FORMAT_S8:
				value = (double)(((char *)data->audio)[counter]) / SCHAR_MAX;
				break;
		}
		
		if (!counter)
			average = value;
		else
			average = (average + value) / 2;
	}
	
	WIILIGHT_SetLevel(fabs(average) * UCHAR_MAX);
	return data;
}

static int af_open(af_instance_t *af)
{
	af->control = control;
	af->uninit = uninit;
	af->play = play;
	af->mul = 1;
	af->data = malloc(sizeof(af_data_t));
	
    if (af->data == NULL)
        return AF_ERROR;
	
	WIILIGHT_Init();
	WIILIGHT_TurnOn();
	
    return AF_OK;
}


af_info_t af_info_wiilight = {
    "Disc slot light",
    "wiilight",
    "Extrems <metaradil@gmail.com>",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
