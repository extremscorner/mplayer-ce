#include <stdio.h>
#include <stdlib.h>
//#include <sys/time.h>
#include <string.h>

#include "config.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"

#include <ogcsys.h>

static ao_info_t info = 
{
	"gekko audio output",
	"gekko",
	"r0n",
	""
};

LIBAO_EXTERN(gekko)

struct	timeval last_tv;
int	buffer;


////////////////////////////////////////////////////////////

//Mplayer audio stuff callback
#define BUF_LEN 8192
#define NB_BUF 32
//#define BUF_LEN 32768
//#define NB_BUF 8
#define BUF_TOTALSIZE (NB_BUF*BUF_LEN)
#define AUDIO_PREBUF (BUF_TOTALSIZE/4)

static int curplay = 0, curwrite = 0;
static int inbuf = 0, audplay = 0;
static u8 audioBuf[NB_BUF][BUF_LEN] ATTRIBUTE_ALIGN(32);
static mutex_t m_audioMutex;
static int m_au_rate, m_au_channels, m_au_bps;

void wii_audio_reset()
{
	LWP_MutexLock(m_audioMutex);
	AUDIO_StopDMA();
	memset( audioBuf, 0, BUF_TOTALSIZE );
	DCFlushRange( audioBuf, BUF_TOTALSIZE );
	curplay = curwrite = 0;
	inbuf = 0; audplay = 0;
	LWP_MutexUnlock(m_audioMutex);
}

int wii_audio_init(int rate, int channels, int bps)
{
	m_au_rate = rate;
	m_au_channels = channels;
	m_au_bps = bps;
	wii_audio_reset();
	return 0;
}

static void dmaCallback()
{	
	LWP_MutexLock(m_audioMutex);
	
	if(audplay)
	{
		AUDIO_StopDMA();
		
		inbuf -= BUF_LEN;
		if(inbuf<0) 
		{
			inbuf = 0;
			audplay = 0;
			LWP_MutexUnlock(m_audioMutex);
			return;
		}
	}
	
	AUDIO_InitDMA( (u32)audioBuf[curplay], BUF_LEN );
	AUDIO_StartDMA();
	curplay++;
	if(curplay>=NB_BUF) curplay = 0;
	
	audplay = 1;
	
	LWP_MutexUnlock(m_audioMutex);
}

int audioGetAvail()
{
	return BUF_TOTALSIZE-inbuf;
}

int wii_audio_get_space(void)
{
	int nb = 0;
	LWP_MutexLock(m_audioMutex);
	nb = audioGetAvail();
	LWP_MutexUnlock(m_audioMutex);
	return nb;
}
int wii_audio_play(char *data,int len,int flags)
{	
	int eat = 0;
	
	LWP_MutexLock(m_audioMutex);
	
	while(len>0)
	{
		int l = len;
		int remain = audioGetAvail();
		if(l>remain) l = remain;
		if(l>BUF_LEN) l = BUF_LEN;
		if(l<BUF_LEN)
			break;
		memcpy(audioBuf[curwrite], data, l);
		DCFlushRange(audioBuf[curwrite], l);
		curwrite++;
		if(curwrite>=NB_BUF) curwrite = 0;
		inbuf+=l;
		
		len -=l;
		data += l;
		eat += l;	
	}
	
	LWP_MutexUnlock(m_audioMutex);	
	
	//launch playback here if passed a certain threshold
	if(!audplay && inbuf>=AUDIO_PREBUF)
	{
		dmaCallback();
	}
		
	return eat;
}
float wii_audio_get_delay(int in_mplayerbuf)
{
	int nb = 0;
	LWP_MutexLock(m_audioMutex);
	if(!audplay)
		nb = inbuf;
	else
		nb = inbuf - (BUF_LEN - AUDIO_GetDMABytesLeft());
	LWP_MutexUnlock(m_audioMutex);
	
	return (float)nb/(float)m_au_bps;
}


////////////////////////////////////////////////////////////






// to set/get/query special features/parameters
static int control(int cmd,void *arg){
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
	AUDIO_Init( NULL );
	LWP_MutexInit(&m_audioMutex, false);
	AUDIO_SetDSPSampleRate( AI_SAMPLERATE_48KHZ );
	AUDIO_RegisterDMACallback( dmaCallback );

	//force resampling to 48khz stereo 16bps big endian
	channels = 2;
	rate = 48000;
	format = AF_FORMAT_S16_BE;
		
    ao_data.buffersize= 65536;
    ao_data.outburst=1024;
    ao_data.channels=channels;
    ao_data.samplerate=rate;
    ao_data.format=format;
/*    ao_data.bps=channels*rate;
    if (format != AF_FORMAT_U8 && format != AF_FORMAT_S8)
	ao_data.bps*=2; 
    buffer=0;
    gettimeofday(&last_tv, 0);
*/
  ao_data.bps=channels*rate*2;
	ao_data.buffersize = wii_audio_init(rate, channels, ao_data.bps);
	ao_data.outburst = 16384;
	
    return 1;
}

// close audio device
static void uninit(int immed){
	LWP_MutexDestroy(m_audioMutex);
	AUDIO_RegisterDMACallback(NULL);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){
    //buffer=0;
    wii_audio_reset();
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
}

// return: how many bytes can be played without blocking
static int get_space(void){

    //drain();
    //return ao_data.buffersize - buffer;
    return wii_audio_get_space();
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
/*
    int maxbursts = (ao_data.buffersize - buffer) / ao_data.outburst;
    int playbursts = len / ao_data.outburst;
    int bursts = playbursts > maxbursts ? maxbursts : playbursts;
    buffer += bursts * ao_data.outburst;
    return bursts * ao_data.outburst;
*/
	if (!(flags & AOPLAY_FINAL_CHUNK))
		len = (len/ao_data.outburst)*ao_data.outburst; 	
	return wii_audio_play(data, len, flags);    
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){

    //drain();
    //return (float) buffer / (float) ao_data.bps;
    return wii_audio_get_delay(ao_data.buffersize);
}
