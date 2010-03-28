/****************************************************************************
*	gx_supp.c - Generic GX Support for Emulators
*	softdev 2007
*	dhewg 2008
*	sepp256 2008 - Coded YUV->RGB conversion in TEV.
*	Tantric 2009 - rewritten using threads, with GUI overlaid
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation; either version 2 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License along
*	with this program; if not, write to the Free Software Foundation, Inc.,
*	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
* NGC GX Video Functions
*
* These are pretty standard functions to setup and use GX scaling.
****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <ogc/mutex.h>
#include <ogc/lwp.h>

#include "gx_supp.h"
#include "ave-rvl.h"
#include "mem2_manager.h"


#define max(a, b) ((a > b) ? b : a)

#define DEFAULT_FIFO_SIZE (256 * 1024)

#define HASPECT 320
#define VASPECT 240


/*** 2D ***/
static u32 whichfb;
static u32 *xfb[2];
GXRModeObj *vmode = NULL;

int screenwidth = 640;
int screenheight = 480;

/*** 3D GX ***/
static u8 *gp_fifo;

/*** Texture memory ***/
static u8 *Ytexture[2] = { NULL, NULL };
static u8 *Utexture[2] = { NULL, NULL };
static u8 *Vtexture[2] = { NULL, NULL };
static bool clear_next = false;

static u32 Ytexsize, UVtexsize;

static GXTexObj YtexObj, UtexObj, VtexObj;
static u16 vwidth, vheight;
static u16 Ywidth, Yheight, UVwidth, UVheight;

/* New texture based scaler */
static f32 square[] ATTRIBUTE_ALIGN(32) = {
	-HASPECT, VASPECT,
	HASPECT, VASPECT,
	HASPECT, -VASPECT,
	-HASPECT, -VASPECT,
};

static GXColor colors[] ATTRIBUTE_ALIGN(32) = {
	{0, 255, 0, 255}		//G
};

static f32 Ytexcoords[] ATTRIBUTE_ALIGN(32) = {
	0.0, 0.0,
	1.0, 0.0,
	1.0, 1.0,
	0.0, 1.0,
};

static f32 UVtexcoords[] ATTRIBUTE_ALIGN(32) = {
	0.0, 0.0,
	1.0, 0.0,
	1.0, 1.0,
	0.0, 1.0,
};


void GX_InitVideo(int video_mode, bool overscan)
{
	VIDEO_Init();
	
	switch(video_mode)
	{
		case 1:		// NTSC (480i)
			vmode = &TVNtsc480IntDf;
			break;
		case 2:		// Progressive (480p)
			vmode = &TVNtsc480Prog;
			break;
		case 3:		// PAL (50Hz)
			vmode = &TVPal574IntDfScale;
			break;
		case 4:		// PAL (60Hz)
			vmode = &TVEurgb60Hz480IntDf;
			break;
		default:
			vmode = VIDEO_GetPreferredMode(NULL);
	}
	
	int videowidth = VI_MAX_WIDTH_NTSC;
	int videoheight = VI_MAX_HEIGHT_NTSC;
	
	if ((vmode->viTVMode >> 2) == VI_PAL)
	{
		videowidth = VI_MAX_WIDTH_PAL;
		videoheight = VI_MAX_HEIGHT_PAL;
	}
	
	if (overscan)
		vmode->viHeight = ceil((float)(videoheight * 0.95) / 8) * 8;
	else
		vmode->viHeight = videoheight;
	
	vmode->xfbHeight = vmode->viHeight;
	vmode->efbHeight = max(vmode->xfbHeight, 528);
	
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
	{
        if (overscan) vmode->viWidth = videowidth * 0.95;
		screenwidth = ((float)screenheight / 9) * 16;
	}
	else
	{
        if (overscan) vmode->viWidth = videowidth * 0.93;
		screenwidth = ((float)screenheight / 3) * 4;
	}
	
	if (overscan)
		vmode->viWidth = ceil((float)vmode->viWidth / 16) * 16;
	else
		vmode->viWidth = videowidth;
	
	vmode->fbWidth = vmode->viWidth;
	
	vmode->viXOrigin = (videowidth - vmode->viWidth) / 2;
	vmode->viYOrigin = (videoheight - vmode->viHeight) / 2;
	
	if (overscan)
	{
		s8 hor_offset = 0;
		
		if (CONF_GetDisplayOffsetH(&hor_offset) > 0)
			vmode->viXOrigin += hor_offset;
	}
	
	VIDEO_Configure(vmode);
	
	xfb[0] = (u32 *)MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	xfb[1] = (u32 *)MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	
	VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);
	
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else
	    while (VIDEO_GetNextField())
	    	VIDEO_WaitVSync();
}

/****************************************************************************
 * draw_initYUV - Internal function to setup TEV for YUV->RGB conversion.
 ****************************************************************************/
static void draw_initYUV(void)
{
	//Setup TEV
	GX_SetNumChans(1);
	GX_SetNumTexGens(3);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY);
	
	//Y'UV->RGB formulation 2
	GX_SetNumTevStages(12);
	GX_SetTevKColor(GX_KCOLOR0, (GXColor){ 255,      0,        0,    18.624});	//R {1, 0, 0, 16*1.164}
	GX_SetTevKColor(GX_KCOLOR1, (GXColor){  0,       0,       255,   41.82});	//B {0, 0, 1, 0.164}
	GX_SetTevKColor(GX_KCOLOR2, (GXColor){203.745, 103.6575,   0,     255});	// {1.598/2, 0.813/2, 0}
	GX_SetTevKColor(GX_KCOLOR3, (GXColor){  0,     24.92625, 128.52,  255});	// {0, 0.391/4, 2.016/4}
	//Stage 0: TEVREG0 <- { 0, 2Um, 2Up }; TEVREG0A <- {16*1.164}
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_RASC, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_2, GX_ENABLE, GX_TEVREG0);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
	GX_SetTevAlphaIn (GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_RASA, GX_CA_KONST, GX_CA_ZERO);
	GX_SetTevAlphaOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVREG0);
	//Stage 1: TEVREG1 <- { 0, 2Up, 2Um };
	GX_SetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K1);
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_KONST, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_2, GX_ENABLE, GX_TEVREG1);
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 2: TEVREG2 <- { Vp, Vm, 0 }
	GX_SetTevKColorSel(GX_TEVSTAGE2, GX_TEV_KCSEL_K0);
	GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD1, GX_TEXMAP2, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE2, GX_CC_RASC, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_ENABLE, GX_TEVREG2);
	GX_SetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 3: TEVPREV <- { (Vm), (Vp), 0 }
	GX_SetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K0);
	GX_SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD1, GX_TEXMAP2, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE3, GX_CC_KONST, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 4: TEVPREV <- { (-1.598Vm), (-0.813Vp), 0 }; TEVPREVA <- {Y' - 16*1.164}
	GX_SetTevKColorSel(GX_TEVSTAGE4, GX_TEV_KCSEL_K2);
	GX_SetTevOrder(GX_TEVSTAGE4, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE4, GX_CC_ZERO, GX_CC_KONST, GX_CC_CPREV, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE4, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_2, GX_DISABLE, GX_TEVPREV);
	GX_SetTevKAlphaSel(GX_TEVSTAGE4, GX_TEV_KASEL_1);
	GX_SetTevAlphaIn(GX_TEVSTAGE4, GX_CA_ZERO, GX_CA_KONST, GX_CA_A0, GX_CA_TEXA);
	GX_SetTevAlphaOp(GX_TEVSTAGE4, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	//Stage 5: TEVPREV <- { -1.598Vm (+1.139/2Vp), -0.813Vp +0.813/2Vm), 0 }; TEVREG1A <- {Y' -16*1.164 - Y'*0.164} = {(Y'-16)*1.164}
	GX_SetTevKColorSel(GX_TEVSTAGE5, GX_TEV_KCSEL_K2);
	GX_SetTevOrder(GX_TEVSTAGE5, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE5, GX_CC_ZERO, GX_CC_KONST, GX_CC_C2, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE5, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	GX_SetTevKAlphaSel(GX_TEVSTAGE5, GX_TEV_KASEL_K1_A);
	GX_SetTevAlphaIn(GX_TEVSTAGE5, GX_CA_ZERO, GX_CA_KONST, GX_CA_TEXA, GX_CA_APREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE5, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVREG1);
	//Stage 6: TEVPREV <- {	-1.598Vm (+1.598Vp), -0.813Vp (+0.813Vm), 0 } = {	(+1.598V), (-0.813V), 0 }
	GX_SetTevKColorSel(GX_TEVSTAGE6, GX_TEV_KCSEL_K2);
	GX_SetTevOrder(GX_TEVSTAGE6, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE6, GX_CC_ZERO, GX_CC_KONST, GX_CC_C2, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE6, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 7: TEVPREV <- {	((Y'-16)*1.164) +1.598V, ((Y'-16)*1.164) -0.813V, ((Y'-16)*1.164) }
	GX_SetTevKColorSel(GX_TEVSTAGE7, GX_TEV_KCSEL_1);
	GX_SetTevOrder(GX_TEVSTAGE7, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE7, GX_CC_ZERO, GX_CC_ONE, GX_CC_A1, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE7, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE7, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE7, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 8: TEVPREV <- {	(Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V (-.394/2Up), (Y'-16)*1.164 (-2.032/2Um)}
	GX_SetTevKColorSel(GX_TEVSTAGE8, GX_TEV_KCSEL_K3);
	GX_SetTevOrder(GX_TEVSTAGE8, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE8, GX_CC_ZERO, GX_CC_KONST, GX_CC_C1, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE8, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE8, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE8, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 9: TEVPREV <- { (Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V (-.394Up), (Y'-16)*1.164 (-2.032Um)}
	GX_SetTevKColorSel(GX_TEVSTAGE9, GX_TEV_KCSEL_K3);
	GX_SetTevOrder(GX_TEVSTAGE9, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE9, GX_CC_ZERO, GX_CC_KONST, GX_CC_C1, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE9, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE9, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE9, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 10: TEVPREV <- { (Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V -.394Up (+.394/2Um), (Y'-16)*1.164 -2.032Um (+2.032/2Up)}
	GX_SetTevKColorSel(GX_TEVSTAGE10, GX_TEV_KCSEL_K3);
	GX_SetTevOrder(GX_TEVSTAGE10, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE10, GX_CC_ZERO, GX_CC_KONST, GX_CC_C0, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE10, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE10, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE10, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 11: TEVPREV <- { (Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V -.394Up (+.394Um), (Y'-16)*1.164 -2.032Um (+2.032Up)} = { (Y'-16)*1.164 +1.139V, (Y'-16)*1.164 -0.58V -.394U, (Y'-16)*1.164 +2.032U}
	GX_SetTevKColorSel(GX_TEVSTAGE11, GX_TEV_KCSEL_K3);
	GX_SetTevOrder(GX_TEVSTAGE11, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE11, GX_CC_ZERO, GX_CC_KONST, GX_CC_C0, GX_CC_CPREV);
	GX_SetTevColorOp(GX_TEVSTAGE11, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevKAlphaSel(GX_TEVSTAGE11, GX_TEV_KASEL_1);
	GX_SetTevAlphaIn(GX_TEVSTAGE11, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	GX_SetTevAlphaOp(GX_TEVSTAGE11, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	
	//Setup vertex description/format
	GX_ClearVtxDesc();
	
	GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_TEX1, GX_INDEX8);
	
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
	
	DCFlushRange(square, sizeof(square));
	DCFlushRange(Ytexcoords, sizeof(Ytexcoords));
	DCFlushRange(UVtexcoords, sizeof(UVtexcoords));
	
	GX_SetArray(GX_VA_POS, square, sizeof(f32) * 2);
	GX_SetArray(GX_VA_CLR0, colors, sizeof(GXColor));
	GX_SetArray(GX_VA_TEX0, Ytexcoords, sizeof(f32) * 2);
	GX_SetArray(GX_VA_TEX1, UVtexcoords, sizeof(f32) * 2);
	
	//init YUV texture objects
	GX_InitTexObj(&YtexObj, Ytexture[whichfb ^ 1], Ywidth, Yheight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&YtexObj, GX_LINEAR, GX_LINEAR, 0.0, 0.0, 0.0, GX_TRUE, GX_TRUE, GX_ANISO_4);
	GX_InitTexObj(&UtexObj, Utexture[whichfb ^ 1], UVwidth, UVheight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&UtexObj, GX_LINEAR, GX_LINEAR, 0.0, 0.0, 0.0, GX_TRUE, GX_TRUE, GX_ANISO_4);
	GX_InitTexObj(&VtexObj, Vtexture[whichfb ^ 1], UVwidth, UVheight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&VtexObj, GX_LINEAR, GX_LINEAR, 0.0, 0.0, 0.0, GX_TRUE, GX_TRUE, GX_ANISO_4);
}

//------- rodries change: to avoid image_buffer intermediate ------
static int w1,w2,h1,h2,df1,df2,old_h1_2=-1;
static int p01,p02,p03,p11,p12,p13;
static u16 Yrowpitch;
static u16 UVrowpitch;
static u64 *Ydst, *Udst, *Vdst;

void getStrideInfo(int *_w1, int *_df1, int *_Yrowpitch)  // for subtitle info
{
	*_w1 = w1;
	*_df1 = df1;
	*_Yrowpitch = Yrowpitch;
}

void GX_ConfigTextureYUV(u16 width, u16 height, u16 *pitch)
{
	GX_ResetTextureYUVPointers();
	
	int half_wd = width / 2;
	int half_ht = height / 2;
	
	Ywidth = ceil((float)width / 8) * 8;
	UVwidth = ceil((float)half_wd / 8) * 8;

    w1 = pitch[0] / 8;
    w2 = pitch[1] / 8;
	
    df1 = ((Ywidth / 8) - w1) * 4;
    df2 = ((UVwidth / 8) - w2) * 4;
	
    Yrowpitch = (pitch[0] / 2) - w1;
	UVrowpitch = (pitch[1] / 2) - w2;
	
	Yheight = ceil((float)height / 4) * 4;
	UVheight = ceil((float)half_ht / 4) * 4;
	
	f32 YtexcoordS = (f32)width / (f32)Ywidth;
	f32 UVtexcoordS = (f32)(half_wd - (half_wd % 2)) / (f32)UVwidth;
	
	if (YtexcoordS < 1.0)
		YtexcoordS -= 0.001f / Ywidth;
	
	if (UVtexcoordS < 1.0)
		UVtexcoordS -= 0.001f / UVwidth;
	
	Ytexcoords[2] = Ytexcoords[4] = YtexcoordS;
	UVtexcoords[2] = UVtexcoords[4] = UVtexcoordS;
	
	f32 YtexcoordT = (f32)height / (f32)Yheight;
	f32 UVtexcoordT = (f32)(half_ht - (half_ht % 2)) / (f32)UVheight;
	
	if (YtexcoordT < 1.0)
		YtexcoordT -= 0.001f / Yheight;
	
	if (UVtexcoordT < 1.0)
		UVtexcoordT -= 0.001f / UVheight;
	
	Ytexcoords[5] = Ytexcoords[7] = YtexcoordT;
	UVtexcoords[5] = UVtexcoords[7] = UVtexcoordT;
	
	/** Update scaling **/
	draw_initYUV();

	p01 = pitch[0];
    p02 = pitch[0] * 2;
    p03 = pitch[0] * 3;
	
    p11 = pitch[1];
    p12 = pitch[1] * 2;
    p13 = pitch[1] * 3;
	
	vwidth = width;
	vheight = height;
    
    GX_UpdateSquare();
}

void GX_UpdatePitch(u16 *pitch)
{
	//black
    memset(Ytexture[whichfb ^ 1], 0, Ytexsize);
	memset(Utexture[whichfb ^ 1], 0x80, UVtexsize);
	memset(Vtexture[whichfb ^ 1], 0x80, UVtexsize);
	clear_next = true;
	
	GX_ConfigTextureYUV(vwidth, vheight, pitch);
}

//nunchuk control
extern float m_screenleft_shift, m_screenright_shift;
extern float m_screentop_shift, m_screenbottom_shift;

static f32 mysquare[12] ATTRIBUTE_ALIGN(32);

void GX_UpdateSquare()
{
	memcpy(mysquare, square, sizeof(square));
	
	mysquare[0] -= m_screenleft_shift * 100;
	mysquare[6] -= m_screenleft_shift * 100;
	mysquare[2] -= m_screenright_shift * 100;
	mysquare[4] -= m_screenright_shift * 100;
	mysquare[1] -= m_screentop_shift * 100;
	mysquare[3] -= m_screentop_shift * 100;
	mysquare[5] -= m_screenbottom_shift * 100;
	mysquare[7] -= m_screenbottom_shift * 100;
	
	DCFlushRange(mysquare, sizeof(mysquare));
	GX_SetArray(GX_VA_POS, mysquare, sizeof(f32) * 2);
}

/****************************************************************************
 * GX_StartYUV - Initialize GX for given width/height.
 ****************************************************************************/
void GX_StartYUV(u16 width, u16 height, f32 haspect, f32 vaspect)
{
	static bool inited = false;
	
	Mtx GXmodelView2D;
	Mtx44 perspective;
	
	/*** Set new aspect ***/
	square[0] = square[6] = -haspect;
	square[2] = square[4] = haspect;
	square[1] = square[3] = vaspect;
	square[5] = square[7] = -vaspect;
	
	if (!Ytexture[0])
		Ytexture[0] = (u8 *)mem2_malign(32, 1024 * 1024);
	if (!Utexture[0])
		Utexture[0] = (u8 *)mem2_malign(32, 512 * 512);
	if (!Vtexture[0])
		Vtexture[0] = (u8 *)mem2_malign(32, 512 * 512);
	
	if (!Ytexture[1])
		Ytexture[1] = (u8 *)mem2_malign(32, 1024 * 1024);
	if (!Utexture[1])
		Utexture[1] = (u8 *)mem2_malign(32, 512 * 512);
	if (!Vtexture[1])
		Vtexture[1] = (u8 *)mem2_malign(32, 512 * 512);
	
	Ywidth = ceil((float)width / 8) * 8;
	Yheight = ceil((float)height / 4) * 4;
	
	Ytexsize = Ywidth * Yheight;
	
	UVwidth = ceil((float)(width / 2) / 8) * 8;
	UVheight = ceil((float)(height / 2) / 4) * 4;
	
	UVtexsize = UVwidth * UVheight;
	
	memset(Ytexture[whichfb ^ 1], 0, Ytexsize);
	memset(Utexture[whichfb ^ 1], 0x80, UVtexsize);
	memset(Vtexture[whichfb ^ 1], 0x80, UVtexsize);
	clear_next = true;
	
	if (!inited)
	{
		/*** Clear out FIFO area ***/
		gp_fifo = (u8 *)memalign(32, DEFAULT_FIFO_SIZE);
		memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);

		/*** Initialise GX ***/
		GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);
		GX_SetCopyClear((GXColor){0, 0, 0, 0xFF}, GX_MAX_Z24);
		GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
		
		f32 yscale = GX_GetYScaleFactor(vmode->efbHeight, vmode->xfbHeight);
		u32 xfbHeight = GX_SetDispCopyYScale(yscale);
		
		GX_SetScissor(0, 0, vmode->fbWidth, vmode->efbHeight);
		GX_SetDispCopySrc(0, 0, max(vmode->fbWidth, 640), vmode->efbHeight);
		GX_SetDispCopyDst(vmode->fbWidth, xfbHeight);
		GX_SetCopyFilter(vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
		GX_SetFieldMode(vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
		
		GX_SetCullMode(GX_CULL_NONE);
		GX_SetClipMode(GX_DISABLE);
		GX_CopyDisp(xfb[whichfb ^ 1], GX_TRUE);
		GX_SetDispCopyGamma(GX_GM_1_0);
		GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_TRUE);
		
		guMtxIdentity(GXmodelView2D);
		guMtxTransApply(GXmodelView2D, GXmodelView2D, 0.0, 0.0, 0.0);
		GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);
		
		guOrtho(perspective, screenheight / 2, -(screenheight / 2), -(screenwidth / 2), screenwidth / 2, 0.0, 1.0);
		GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);
		
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_ENABLE);
		GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
		GX_SetColorUpdate(GX_ENABLE);
		
		GX_SetDrawDoneCallback(VIDEO_Flush);
		GX_Flush();
		GX_UpdateSquare();
		
		inited = true;
	}
}

void GX_FillTextureYUV(u16 height, u8 *buffer[3])
{
	u64 *Ysrc1 = (u64 *)buffer[0];
	u64 *Ysrc2 = (u64 *)(buffer[0] + p01);
	u64 *Ysrc3 = (u64 *)(buffer[0] + p02);
	u64 *Ysrc4 = (u64 *)(buffer[0] + p03);
	
	u64 *Usrc1 = (u64 *)buffer[1];
	u64 *Usrc2 = (u64 *)(buffer[1] + p11);
	u64 *Usrc3 = (u64 *)(buffer[1] + p12);
	u64 *Usrc4 = (u64 *)(buffer[1] + p13);
	
	u64 *Vsrc1 = (u64 *)buffer[2];
	u64 *Vsrc2 = (u64 *)(buffer[2] + p11);
	u64 *Vsrc3 = (u64 *)(buffer[2] + p12);
	u64 *Vsrc4 = (u64 *)(buffer[2] + p13);

	if (height != old_h1_2)
	{
		old_h1_2 = height;
		h1 = ceil((float)height / 4);
    	h2 = ceil((float)(height / 2) / 4);
	}

	// Copy strides into plain texture data.
	// Luminance (Y) plane.
	for (int h = 0; h < h1; h++)
	{
		for (int w = 0; w < w1; w++)
		{
			*Ydst++ = *Ysrc1++;
			*Ydst++ = *Ysrc2++;
			*Ydst++ = *Ysrc3++;
			*Ydst++ = *Ysrc4++;
		}
		
		Ydst += df1;
		
		Ysrc1 += Yrowpitch;
		Ysrc2 += Yrowpitch;
		Ysrc3 += Yrowpitch;
		Ysrc4 += Yrowpitch;
	}

	// Chrominance (U&V) planes.
	for (int h = 0; h < h2; h++)
	{
		for (int w = 0; w < w2; w++)
		{
			*Udst++ = *Usrc1++;
			*Udst++ = *Usrc2++;
			*Udst++ = *Usrc3++;
			*Udst++ = *Usrc4++;
			
			*Vdst++ = *Vsrc1++;
			*Vdst++ = *Vsrc2++;
			*Vdst++ = *Vsrc3++;
			*Vdst++ = *Vsrc4++;
		}
		
		Udst += df2;
		Vdst += df2;
		
		Usrc1 += UVrowpitch;
		Usrc2 += UVrowpitch;
		Usrc3 += UVrowpitch;
		Usrc4 += UVrowpitch;
		
		Vsrc1 += UVrowpitch;
		Vsrc2 += UVrowpitch;
		Vsrc3 += UVrowpitch;
		Vsrc4 += UVrowpitch;
	}
}

void GX_RenderTexture(bool vsync)
{
	static bool first_frame = true;
	
	if (!first_frame)
	{
		GX_WaitDrawDone();
		
		if (vsync)
		{
			VIDEO_WaitVSync();
			
			if (vmode->viTVMode & VI_NON_INTERLACE)
				VIDEO_WaitVSync();
		}
		
		if (clear_next)
		{
			memset(Ytexture[whichfb], 0, Ytexsize);
			memset(Utexture[whichfb], 0x80, UVtexsize);
			memset(Vtexture[whichfb], 0x80, UVtexsize);
			clear_next = false;
		}
	}
	
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	
	whichfb ^= 1;
	
	DCFlushRange(Ytexture[whichfb], Ytexsize);
	DCFlushRange(Utexture[whichfb], UVtexsize);
	DCFlushRange(Vtexture[whichfb], UVtexsize);
	
	GX_InitTexObjData(&YtexObj, Ytexture[whichfb]);
	GX_InitTexObjData(&UtexObj, Utexture[whichfb]);
	GX_InitTexObjData(&VtexObj, Vtexture[whichfb]);
	
	GX_LoadTexObj(&YtexObj, GX_TEXMAP0);	// MAP0 <- Y
	GX_LoadTexObj(&UtexObj, GX_TEXMAP1);	// MAP1 <- U
	GX_LoadTexObj(&VtexObj, GX_TEXMAP2);	// MAP2 <- V
	
	u16 xfb_copypt = vmode->fbWidth / 2;
	u16 efb_drawpt = ceil((float)xfb_copypt / 16) * 16;
	int difference = efb_drawpt - xfb_copypt;
	
	for (int x = 0; x < 2; x++)
	{
		u16 efb_offset = (xfb_copypt - difference) * x;
		
		GX_SetScissorBoxOffset(efb_offset, 0);
		GX_SetDispCopySrc(0, 0, efb_drawpt, vmode->efbHeight);
		
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			GX_Position1x8(0); GX_Color1x8(0); GX_TexCoord1x8(0); GX_TexCoord1x8(0);
			GX_Position1x8(1); GX_Color1x8(0); GX_TexCoord1x8(1); GX_TexCoord1x8(1);
			GX_Position1x8(2); GX_Color1x8(0); GX_TexCoord1x8(2); GX_TexCoord1x8(2);
			GX_Position1x8(3); GX_Color1x8(0); GX_TexCoord1x8(3); GX_TexCoord1x8(3);
		GX_End();
		
		u32 xfb_offset = (xfb_copypt * VI_DISPLAY_PIX_SZ) * x;
		GX_CopyDisp((void *)((u32)xfb[whichfb] + xfb_offset), GX_TRUE);
	}
	
	GX_SetDrawDone();
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	first_frame = false;
}

void GX_ResetTextureYUVPointers()
{
	Ydst = (u64 *)Ytexture[whichfb ^ 1];
	Udst = (u64 *)Utexture[whichfb ^ 1];
	Vdst = (u64 *)Vtexture[whichfb ^ 1];
}

u8 *GetYtexture() { return Ytexture[whichfb ^ 1]; }
u16 GetYrowpitch() { return Yrowpitch; }
u16 GetYrowpitchDf() { return Yrowpitch + df1; }
