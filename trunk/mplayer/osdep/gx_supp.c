/****************************************************************************
*	gx_supp.c - Generic GX Support for Emulators
*	softdev 2007
*	dhewg 2008
*	sepp256 2008 - Coded YUV->RGB conversion in TEV.
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

#include "gx_supp.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)

#define HASPECT 320
#define VASPECT 240

#ifdef __cplusplus
extern "C" {
#endif

/*** 2D ***/
static u32 whichfb;
static u32 *xfb[2];
static bool component_fix=false;
GXRModeObj *vmode = NULL;

/*** 3D GX ***/
static u8 *gp_fifo;

/*** Texture memory ***/
static u8 *texturemem = NULL,*Ytexture = NULL,*Utexture = NULL,*Vtexture = NULL;
static u32 texturesize,Ytexsize,UVtexsize;

GXTexObj texobj,YtexObj,UtexObj,VtexObj;
static Mtx view;
static u16 vwidth, vheight, oldvwidth, oldvheight;
static u16 Ywidth, Yheight, UVwidth, UVheight;

/* New texture based scaler */
typedef struct tagcamera {
	Vector pos;
	Vector up;
	Vector view;
} camera;

static s16 square[] ATTRIBUTE_ALIGN(32) = {
	-HASPECT, VASPECT, 0,
	HASPECT, VASPECT, 0,
	HASPECT, -VASPECT, 0,
	-HASPECT, -VASPECT, 0,
};

static GXColor colors[] ATTRIBUTE_ALIGN(32) = {
	{0,255,0,255}		//G
};

static u8 texcoords[] ATTRIBUTE_ALIGN(32) = {
	0x00, 0x00,
	0x01, 0x00,
	0x01, 0x01,
	0x00, 0x01,
};

static camera cam = {
	{ 0.0f, 0.0f, 370.0f },
	{ 0.0f, 0.5f, 0.0f },
	{ 0.0f, 0.0f, -0.5f }
};

void GX_InitVideo() {
	vmode = VIDEO_GetPreferredMode(NULL);

  if(!component_fix) vmode->viWidth = 688;
	else 
    vmode->viWidth = VI_MAX_WIDTH_PAL-12;
  
	vmode->viXOrigin = ((VI_MAX_WIDTH_PAL - vmode->viWidth) / 2) + 2;
	
	VIDEO_Configure(vmode);

	xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer(vmode));
	xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer(vmode));
	gp_fifo = (u8 *) memalign(32, DEFAULT_FIFO_SIZE);

	VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

	whichfb = 0;
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
}

void GX_SetComponentFix(bool f) {
	component_fix = f;
}

void GX_SetCamPosZ(float f) {
	cam.pos.z = f;
}

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
static void draw_init(void) {
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetArray(GX_VA_POS, square, 3 * sizeof(s16));

	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_InvalidateTexAll();

	GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565,
					GX_CLAMP, GX_CLAMP, GX_FALSE);
}

static void draw_vert(u8 pos, u8 c, f32 s, f32 t) {
	GX_Position1x8(pos);
	GX_Color1x8(c);
	GX_TexCoord2f32(s, t);
}

static void draw_square(Mtx v) {
	Mtx m;
	Mtx mv;

	guMtxIdentity(m);
	guMtxTransApply(m, m, 0, 0, -100);
	guMtxConcat(v, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	draw_vert(0, 0, 0.0, 0.0);
	draw_vert(1, 0, 1.0, 0.0);
	draw_vert(2, 0, 1.0, 1.0);
	draw_vert(3, 0, 0.0, 1.0);
	GX_End();
}

/****************************************************************************
 * StartGX
 ****************************************************************************/
void GX_Start(u16 width, u16 height, s16 haspect, s16 vaspect) {
	static bool inited = false;

	Mtx p;
	GXColor gxbackground = { 0, 0, 0, 0xff };

	/*** Set new aspect ***/
	square[0] = square[9] = -haspect;
	square[3] = square[6] = haspect;
	square[1] = square[4] = vaspect;
	square[7] = square[10] = -vaspect;

	/*** Allocate 32byte aligned texture memory ***/
	texturesize = (width * height) * 2;
	if (texturemem)
		free (texturemem);

	texturemem = (u8 *) memalign(32, texturesize);

	memset(texturemem, 0, texturesize);

	/*** Setup for first call to scaler ***/
	oldvwidth = oldvheight = -1;

	if (inited)
		return;

	inited = true;

	/*** Clear out FIFO area ***/
	memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear(gxbackground, 0x00ffffff);

	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopySrc(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter(vmode->aa, vmode->sample_pattern, GX_TRUE,
						vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,
					((vmode->viHeight == 2 * vmode->xfbHeight) ?
					GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(xfb[whichfb ^ 1], GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	guPerspective(p, 60, 1.33f, 10.0f, 1000.0f);
	GX_LoadProjectionMtx(p, GX_PERSPECTIVE);

	GX_Flush();
}

/****************************************************************************
* GX_Render
*
* Pass in a buffer, width and height to update as a tiled RGB565 texture
****************************************************************************/
void GX_Render(u16 width, u16 height, u8 *buffer, u16 pitch) {
	u16 h, w;
	u64 *dst = (u64 *) texturemem;
	u64 *src1 = (u64 *) buffer;
	u64 *src2 = (u64 *) (buffer + pitch);
	u64 *src3 = (u64 *) (buffer + (pitch * 2));
	u64 *src4 = (u64 *) (buffer + (pitch * 3));
	u16 rowpitch = (pitch >> 3) * 3 + pitch % 8;

	vwidth = width;
	vheight = height;

	whichfb ^= 1;

	if ((oldvheight != vheight) || (oldvwidth != vwidth)) {
		/** Update scaling **/
		oldvwidth = vwidth;
		oldvheight = vheight;
		draw_init();
		memset(&view, 0, sizeof(Mtx));
		guLookAt(view, &cam.pos, &cam.up, &cam.view);
		GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	}

	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_SetTevOp(GX_TEVSTAGE0, GX_DECAL);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	for (h = 0; h < vheight; h += 4) {
		for (w = 0; w < (vwidth >> 2); w++) {
			*dst++ = *src1++;
			*dst++ = *src2++;
			*dst++ = *src3++;
			*dst++ = *src4++;
		}

		src1 += rowpitch;
		src2 += rowpitch;
		src3 += rowpitch;
		src4 += rowpitch;
	}

	DCFlushRange(texturemem, texturesize);

	GX_SetNumChans(1);
	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	draw_square(view);

	GX_DrawDone();

	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(xfb[whichfb], GX_TRUE);
	GX_Flush();

	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
}

/****************************************************************************
 * GX_StartYUV - Initialize GX for given width/height.
 ****************************************************************************/
void GX_StartYUV(u16 width, u16 height, s16 haspect, s16 vaspect) {
	static bool inited = false;

	Mtx p;
	GXColor gxbackground = { 0, 0, 0, 0xff };

	/*** Set new aspect ***/
	square[0] = square[9] = -haspect;
	square[3] = square[6] = haspect;
	square[1] = square[4] = vaspect;
	square[7] = square[10] = -vaspect;

	/*** Allocate 32byte aligned texture memory ***/
	Ytexsize = (width*height);
	UVtexsize = (width*height)/4;
	if (Ytexture)
		free (Ytexture);
	if (Utexture)
		free (Utexture);
	if (Vtexture)
		free (Vtexture);

	Ytexture = (u8 *) memalign(32,Ytexsize);
	Utexture = (u8 *) memalign(32,UVtexsize);
	Vtexture = (u8 *) memalign(32,UVtexsize);

	memset(Ytexture, 0, Ytexsize);
	memset(Utexture, 0, UVtexsize);
	memset(Vtexture, 0, UVtexsize);

	/*** Setup for first call to scaler ***/
	oldvwidth = oldvheight = -1;

	if (inited)
		return;

	inited = true;

	/*** Clear out FIFO area ***/
	memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);
	GX_SetCopyClear(gxbackground, 0x00ffffff);

	GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	GX_SetDispCopyYScale((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetScissor(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopySrc(0, 0, vmode->fbWidth, vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth, vmode->xfbHeight);
	GX_SetCopyFilter(vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(xfb[whichfb ^ 1], GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	guPerspective(p, 60, 1.33f, 10.0f, 1000.0f);
	GX_LoadProjectionMtx(p, GX_PERSPECTIVE);

	GX_Flush();
}

/****************************************************************************
 * draw_initYUV - Internal function to setup TEV for YUV->RGB conversion.
 ****************************************************************************/
void draw_initYUV(void){
	//Setup TEV
	GX_SetNumChans (1);
	GX_SetNumTexGens (3);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY);
#if 0
	//Y'UV->RGB formulation 1
	GX_SetNumTevStages(12);
	GX_SetTevKColor(GX_KCOLOR0, (GXColor) {255,   0,   0, 255});	//R
	GX_SetTevKColor(GX_KCOLOR1, (GXColor) {  0,   0, 255, 255});	//B
	GX_SetTevKColor(GX_KCOLOR2, (GXColor) {145,  74,   0, 255});	// {1.13982/2, 0.5806/2, 0}
	GX_SetTevKColor(GX_KCOLOR3, (GXColor) {  0,  25, 130, 255});	// {0, 0.39465/4, 2.03211/4}
	//Stage 0: TEVREG0 <- { 0, 2Um, 2Up }
		GX_SetTevKColorSel(GX_TEVSTAGE0,GX_TEV_KCSEL_K1);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE0, GX_CC_RASC, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_2, GX_ENABLE, GX_TEVREG0);
		GX_SetTevAlphaIn (GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 1: TEVREG1 <- { 0, 2Up, 2Um }
		GX_SetTevKColorSel(GX_TEVSTAGE1,GX_TEV_KCSEL_K1);
		GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE1, GX_CC_KONST, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_2, GX_ENABLE, GX_TEVREG1);
		GX_SetTevAlphaIn (GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 2: TEVREG2 <- { Vp, Vm, 0 }
		GX_SetTevKColorSel(GX_TEVSTAGE2,GX_TEV_KCSEL_K0);
		GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD1, GX_TEXMAP2,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE2, GX_CC_RASC, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_ENABLE, GX_TEVREG2);
		GX_SetTevAlphaIn (GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 3: TEVPREV <- { (Vm), (Vp), 0 }
		GX_SetTevKColorSel(GX_TEVSTAGE3,GX_TEV_KCSEL_K0);
		GX_SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD1, GX_TEXMAP2,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE3, GX_CC_KONST, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 4: TEVPREV <- { (-1.139Vm), (-0.58Vp), 0 }
		GX_SetTevKColorSel(GX_TEVSTAGE4,GX_TEV_KCSEL_K2);
		GX_SetTevOrder(GX_TEVSTAGE4, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE4, GX_CC_ZERO, GX_CC_KONST, GX_CC_CPREV, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE4, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_2, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE4, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE4, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 5: TEVPREV <- { (Y') -1.139Vm, (Y') -0.58Vp, (Y') }
		GX_SetTevKColorSel(GX_TEVSTAGE5,GX_TEV_KCSEL_1);
		GX_SetTevOrder(GX_TEVSTAGE5, GX_TEXCOORD0, GX_TEXMAP0,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE5, GX_CC_ZERO, GX_CC_ONE, GX_CC_TEXC, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE5, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE5, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE5, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 6: TEVPREV <- {	Y' -1.139Vm (+1.139/2Vp), Y' -0.58Vp (+0.58/2Vm), Y' }
		GX_SetTevKColorSel(GX_TEVSTAGE6,GX_TEV_KCSEL_K2);
		GX_SetTevOrder(GX_TEVSTAGE6, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE6, GX_CC_ZERO, GX_CC_KONST, GX_CC_C2, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE6, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 7: TEVPREV <- {	Y' -1.139Vm (+1.139Vp), Y' -0.58Vp (+0.58Vm), Y' } = {	Y' (+1.139V), Y' (-0.58V), Y' }
		GX_SetTevKColorSel(GX_TEVSTAGE7,GX_TEV_KCSEL_K2);
		GX_SetTevOrder(GX_TEVSTAGE7, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE7, GX_CC_ZERO, GX_CC_KONST, GX_CC_C2, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE7, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE7, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE7, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 8: TEVPREV <- {	Y' +1.139V, Y' -0.58V (-.394/2Up), Y' (-2.032/2Um)}
		GX_SetTevKColorSel(GX_TEVSTAGE8,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE8, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE8, GX_CC_ZERO, GX_CC_KONST, GX_CC_C1, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE8, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE8, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE8, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 9: TEVPREV <- { Y' +1.139V, Y' -0.58V (-.394Up), Y' (-2.032Um)}
		GX_SetTevKColorSel(GX_TEVSTAGE9,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE9, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE9, GX_CC_ZERO, GX_CC_KONST, GX_CC_C1, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE9, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE9, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE9, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 10: TEVPREV <- { Y' +1.139V, Y' -0.58V -.394Up (+.394/2Um), Y' -2.032Um (+2.032/2Up)}
		GX_SetTevKColorSel(GX_TEVSTAGE10,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE10, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE10, GX_CC_ZERO, GX_CC_KONST, GX_CC_C0, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE10, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE10, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE10, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 11: TEVPREV <- { Y' +1.139V, Y' -0.58V -.394Up (+.394Um), Y' -2.032Um (+2.032Up)} = { Y' +1.139V, Y' -0.58V -.394U, Y' +2.032U}
		GX_SetTevKColorSel(GX_TEVSTAGE11,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE11, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE11, GX_CC_ZERO, GX_CC_KONST, GX_CC_C0, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE11, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
		GX_SetTevKAlphaSel(GX_TEVSTAGE11,GX_TEV_KASEL_1);
		GX_SetTevAlphaIn (GX_TEVSTAGE11, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
		GX_SetTevAlphaOp (GX_TEVSTAGE11, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
#else
	//Y'UV->RGB formulation 2
	GX_SetNumTevStages(12);
	GX_SetTevKColor(GX_KCOLOR0, (GXColor) {255,   0,   0,  19});	//R {1, 0, 0, 16*1.164} 
	GX_SetTevKColor(GX_KCOLOR1, (GXColor) {  0,   0, 255,  42});	//B {0, 0, 1, 0.164}
	GX_SetTevKColor(GX_KCOLOR2, (GXColor) {204,  104,   0, 255});	// {1.598/2, 0.813/2, 0}
	GX_SetTevKColor(GX_KCOLOR3, (GXColor) {  0,  25, 129, 255});	// {0, 0.391/4, 2.016/4}
	//Stage 0: TEVREG0 <- { 0, 2Um, 2Up }; TEVREG0A <- {16*1.164}
		GX_SetTevKColorSel(GX_TEVSTAGE0,GX_TEV_KCSEL_K1);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE0, GX_CC_RASC, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_2, GX_ENABLE, GX_TEVREG0);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0,GX_TEV_KASEL_K0_A);
		GX_SetTevAlphaIn (GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_RASA, GX_CA_KONST, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVREG0);
	//Stage 1: TEVREG1 <- { 0, 2Up, 2Um }; 
		GX_SetTevKColorSel(GX_TEVSTAGE1,GX_TEV_KCSEL_K1);
		GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE1, GX_CC_KONST, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_2, GX_ENABLE, GX_TEVREG1);
		GX_SetTevAlphaIn (GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 2: TEVREG2 <- { Vp, Vm, 0 }
		GX_SetTevKColorSel(GX_TEVSTAGE2,GX_TEV_KCSEL_K0);
		GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD1, GX_TEXMAP2,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE2, GX_CC_RASC, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_ENABLE, GX_TEVREG2);
		GX_SetTevAlphaIn (GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 3: TEVPREV <- { (Vm), (Vp), 0 }
		GX_SetTevKColorSel(GX_TEVSTAGE3,GX_TEV_KCSEL_K0);
		GX_SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD1, GX_TEXMAP2,GX_COLOR0A0);
		GX_SetTevColorIn (GX_TEVSTAGE3, GX_CC_KONST, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 4: TEVPREV <- { (-1.598Vm), (-0.813Vp), 0 }; TEVPREVA <- {Y' - 16*1.164}
		GX_SetTevKColorSel(GX_TEVSTAGE4,GX_TEV_KCSEL_K2);
		GX_SetTevOrder(GX_TEVSTAGE4, GX_TEXCOORD0, GX_TEXMAP0,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE4, GX_CC_ZERO, GX_CC_KONST, GX_CC_CPREV, GX_CC_ZERO);
		GX_SetTevColorOp (GX_TEVSTAGE4, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_2, GX_DISABLE, GX_TEVPREV);
		GX_SetTevKAlphaSel(GX_TEVSTAGE4,GX_TEV_KASEL_1);
		GX_SetTevAlphaIn (GX_TEVSTAGE4, GX_CA_ZERO, GX_CA_KONST, GX_CA_A0, GX_CA_TEXA);
		GX_SetTevAlphaOp (GX_TEVSTAGE4, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
	//Stage 5: TEVPREV <- { -1.598Vm (+1.139/2Vp), -0.813Vp +0.813/2Vm), 0 }; TEVREG1A <- {Y' -16*1.164 - Y'*0.164} = {(Y'-16)*1.164}
		GX_SetTevKColorSel(GX_TEVSTAGE5,GX_TEV_KCSEL_K2);
		GX_SetTevOrder(GX_TEVSTAGE5, GX_TEXCOORD0, GX_TEXMAP0,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE5, GX_CC_ZERO, GX_CC_KONST, GX_CC_C2, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE5, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevKAlphaSel(GX_TEVSTAGE5,GX_TEV_KASEL_K1_A);
		GX_SetTevAlphaIn (GX_TEVSTAGE5, GX_CA_ZERO, GX_CA_KONST, GX_CA_TEXA, GX_CA_APREV);
		GX_SetTevAlphaOp (GX_TEVSTAGE5, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVREG1);
	//Stage 6: TEVPREV <- {	-1.598Vm (+1.598Vp), -0.813Vp (+0.813Vm), 0 } = {	(+1.598V), (-0.813V), 0 }
		GX_SetTevKColorSel(GX_TEVSTAGE6,GX_TEV_KCSEL_K2);
		GX_SetTevOrder(GX_TEVSTAGE6, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE6, GX_CC_ZERO, GX_CC_KONST, GX_CC_C2, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE6, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 7: TEVPREV <- {	((Y'-16)*1.164) +1.598V, ((Y'-16)*1.164) -0.813V, ((Y'-16)*1.164) }
		GX_SetTevKColorSel(GX_TEVSTAGE7,GX_TEV_KCSEL_1);
		GX_SetTevOrder(GX_TEVSTAGE7, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE7, GX_CC_ZERO, GX_CC_ONE, GX_CC_A1, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE7, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE7, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE7, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 8: TEVPREV <- {	(Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V (-.394/2Up), (Y'-16)*1.164 (-2.032/2Um)}
		GX_SetTevKColorSel(GX_TEVSTAGE8,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE8, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE8, GX_CC_ZERO, GX_CC_KONST, GX_CC_C1, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE8, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE8, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE8, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 9: TEVPREV <- { (Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V (-.394Up), (Y'-16)*1.164 (-2.032Um)}
		GX_SetTevKColorSel(GX_TEVSTAGE9,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE9, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE9, GX_CC_ZERO, GX_CC_KONST, GX_CC_C1, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE9, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE9, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE9, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 10: TEVPREV <- { (Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V -.394Up (+.394/2Um), (Y'-16)*1.164 -2.032Um (+2.032/2Up)}
		GX_SetTevKColorSel(GX_TEVSTAGE10,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE10, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE10, GX_CC_ZERO, GX_CC_KONST, GX_CC_C0, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE10, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_DISABLE, GX_TEVPREV);
		GX_SetTevAlphaIn (GX_TEVSTAGE10, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
		GX_SetTevAlphaOp (GX_TEVSTAGE10, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	//Stage 11: TEVPREV <- { (Y'-16)*1.164 +1.598V, (Y'-16)*1.164 -0.813V -.394Up (+.394Um), (Y'-16)*1.164 -2.032Um (+2.032Up)} = { (Y'-16)*1.164 +1.139V, (Y'-16)*1.164 -0.58V -.394U, (Y'-16)*1.164 +2.032U}
		GX_SetTevKColorSel(GX_TEVSTAGE11,GX_TEV_KCSEL_K3);
		GX_SetTevOrder(GX_TEVSTAGE11, GX_TEXCOORDNULL, GX_TEXMAP_NULL,GX_COLORNULL);
		GX_SetTevColorIn (GX_TEVSTAGE11, GX_CC_ZERO, GX_CC_KONST, GX_CC_C0, GX_CC_CPREV);
		GX_SetTevColorOp (GX_TEVSTAGE11, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
		GX_SetTevKAlphaSel(GX_TEVSTAGE11,GX_TEV_KASEL_1);
		GX_SetTevAlphaIn (GX_TEVSTAGE11, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
		GX_SetTevAlphaOp (GX_TEVSTAGE11, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
#endif //Y'UV->RGB formulation 2

	//Setup blending
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR); //Fix src alpha
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);

	//Setup vertex description/format
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_TEX1, GX_INDEX8);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_U8, 0);

	GX_SetArray(GX_VA_POS, square, 3 * sizeof(s16));
	GX_SetArray(GX_VA_CLR0, colors, sizeof(GXColor));
	GX_SetArray(GX_VA_TEX0, texcoords, 2 * sizeof(u8));
	GX_SetArray(GX_VA_TEX1, texcoords, 2 * sizeof(u8));

	//init YUV texture objects
	GX_InitTexObj(&YtexObj, Ytexture, (u16) Ywidth, (u16) Yheight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE); 
	GX_InitTexObj(&UtexObj, Utexture, (u16) UVwidth, (u16) UVheight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE); 
	GX_InitTexObj(&VtexObj, Vtexture, (u16) UVwidth, (u16) UVheight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE); 
}

/****************************************************************************
* GX_Render - Pass in 3 buffers (Y',U,V planes) and their respective pitches.
****************************************************************************/
void GX_RenderYUV(u16 width, u16 height, u8 *buffer[3], u16 *pitch) {
	Mtx m, mv;
	u16 h, w;
	u64 *Ydst = (u64 *) Ytexture;
	u64 *Udst = (u64 *) Utexture;
	u64 *Vdst = (u64 *) Vtexture;
	u64 *Ysrc1 = (u64 *) buffer[0];
	u64 *Ysrc2 = (u64 *) (buffer[0] + pitch[0]);
	u64 *Ysrc3 = (u64 *) (buffer[0] + (pitch[0] * 2));
	u64 *Ysrc4 = (u64 *) (buffer[0] + (pitch[0] * 3));
	u64 *Usrc1 = (u64 *) buffer[1];
	u64 *Usrc2 = (u64 *) (buffer[1] + pitch[1]);
	u64 *Usrc3 = (u64 *) (buffer[1] + (pitch[1] * 2));
	u64 *Usrc4 = (u64 *) (buffer[1] + (pitch[1] * 3));
	u64 *Vsrc1 = (u64 *) buffer[2];
	u64 *Vsrc2 = (u64 *) (buffer[2] + pitch[2]);
	u64 *Vsrc3 = (u64 *) (buffer[2] + (pitch[2] * 2));
	u64 *Vsrc4 = (u64 *) (buffer[2] + (pitch[2] * 3));
	u16 Yrowpitch = (pitch[0] >> 3) * 3 + pitch[0] % 8;
	u16 UVrowpitch = (pitch[1] >> 3) * 3 + pitch[1] % 8;

	vwidth = width;
	Ywidth = vwidth;
	UVwidth = vwidth>>1;
	vheight = height;
	Yheight = vheight;
	UVheight = vheight>>1;

	whichfb ^= 1;

	if ((oldvheight != vheight) || (oldvwidth != vwidth)) {
		/** Update scaling **/
		oldvwidth = vwidth;
		oldvheight = vheight;
		draw_initYUV();
		memset(&view, 0, sizeof(Mtx));
		guLookAt(view, &cam.pos, &cam.up, &cam.view);
		guMtxIdentity(m);
		guMtxTransApply(m, m, 0, 0, -100);
		guMtxConcat(view, m, mv);
		GX_LoadPosMtxImm(mv, GX_PNMTX0);
		GX_SetViewport(0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	}

	GX_InvVtxCache();
	GX_InvalidateTexAll();

	//Convert YUV frame to GX textures
	//Convert Y plane to texture
	for (h = 0; h < vheight; h+=4) {
		for (w = 0; w < (vwidth >>3); w++) {
			*Ydst++ = *Ysrc1++;
			*Ydst++ = *Ysrc2++;
			*Ydst++ = *Ysrc3++;
			*Ydst++ = *Ysrc4++;
		}
		Ysrc1 += Yrowpitch;
		Ysrc2 += Yrowpitch;
		Ysrc3 += Yrowpitch;
		Ysrc4 += Yrowpitch;
	}
	//Convert U&V planes to textures
	for (h = 0; h < vheight >> 1; h+=4) {
		for (w = 0; w < (vwidth >> 4); w++) {
			*Udst++ = *Usrc1++;
			*Udst++ = *Usrc2++;
			*Udst++ = *Usrc3++;
			*Udst++ = *Usrc4++;
			*Vdst++ = *Vsrc1++;
			*Vdst++ = *Vsrc2++;
			*Vdst++ = *Vsrc3++;
			*Vdst++ = *Vsrc4++;
		}
		Usrc1 += UVrowpitch;
		Usrc2 += UVrowpitch;
		Usrc3 += UVrowpitch;
		Usrc4 += UVrowpitch;
		Vsrc1 += UVrowpitch;
		Vsrc2 += UVrowpitch;
		Vsrc3 += UVrowpitch;
		Vsrc4 += UVrowpitch;
	}

	DCFlushRange(Ytexture, Ytexsize);
	DCFlushRange(Utexture, UVtexsize);
	DCFlushRange(Vtexture, UVtexsize);

	GX_LoadTexObj(&YtexObj, GX_TEXMAP0);	// MAP0 <- Y
	GX_LoadTexObj(&UtexObj, GX_TEXMAP1);	// MAP1 <- U	
	GX_LoadTexObj(&VtexObj, GX_TEXMAP2);	// MAP2 <- V

	//render textures
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position1x8(0); GX_Color1x8(0); GX_TexCoord1x8(0); GX_TexCoord1x8(0);
		GX_Position1x8(1); GX_Color1x8(0); GX_TexCoord1x8(1); GX_TexCoord1x8(1);
		GX_Position1x8(2); GX_Color1x8(0); GX_TexCoord1x8(2); GX_TexCoord1x8(2);
		GX_Position1x8(3); GX_Color1x8(0); GX_TexCoord1x8(3); GX_TexCoord1x8(3);
	GX_End();

	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(xfb[whichfb], GX_TRUE);
	GX_DrawDone();

	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
}

#ifdef __cplusplus
}
#endif

