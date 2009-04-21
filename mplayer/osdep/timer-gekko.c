/*
   MPlayer timer for Wii

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


#include <unistd.h>

#include <ogc/lwp_watchdog.h>

const char *timer_name = "gekko";

int usec_sleep(int usec_delay) {
	return usleep(usec_delay);
}

unsigned int GetTimer(void) {
	return ticks_to_microsecs(gettime());
}

unsigned int GetTimerMS(void) {
	return ticks_to_millisecs(gettime());
}

static s64 relative = 0;
static unsigned int RelativeTime=0;

float GetRelativeTime1(void){
unsigned int t,r;
  t=GetTimer();
//  t*=16;printf("time=%ud\n",t);
  r=t-RelativeTime;
  RelativeTime=t;
  return (float)r * 0.000001F;
}

float GetRelativeTime(void) {
	s64 t;
	float res;

	t = gettime();
	res = (float) ticks_to_nanosecs(diff_ticks(relative, t)) /
			(float) TB_NSPERSEC;
	relative = t;

	return res;
}

void InitTimer(void) {
	GetRelativeTime();
	//relative = gettime();
}

