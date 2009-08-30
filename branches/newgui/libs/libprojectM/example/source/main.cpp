/**
 * main.cpp - projectM visualisation example.
 *
 * Copyright (c) 2009 Rhys "Shareese" Koedijk
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <gctypes.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <fat.h>

#include <projectM.hpp>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define MAX_AUDIO_SAMPLES       512
#define DEFAULT_FIFO_SIZE       256 * 1024

GXRModeObj *rmode = NULL;
void *xfb[2] = { NULL, NULL };
u32 fb = 0;

unsigned char gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);

projectM *projM = NULL;
float fakePCM[MAX_AUDIO_SAMPLES] = { 0 };
bool texturedRender = false;

void displayInit (void)
{
    // Initialise the video system
    VIDEO_Init();

    // Obtain the preferred video mode from the system
    rmode = VIDEO_GetPreferredMode(NULL);
    
    // Widescreen fix
    if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
        rmode->viWidth = VI_MAX_WIDTH_PAL - 12;
        rmode->viXOrigin = ((VI_MAX_WIDTH_PAL - rmode->viWidth) / 2) + 2;
    }
    
    // Set the video mode
    VIDEO_Configure(rmode);
    
    // Allocate the framebuffers (double buffered)
    xfb[0] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    
    // Clear the framebuffers and line up the first buffer for display
    VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
    VIDEO_SetNextFramebuffer(xfb[0]);

    // Flush the display
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    //=================================================================
    
    memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);
    
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE:GX_DISABLE));
    
    GX_SetCullMode(GX_CULL_ALL);
    GX_CopyDisp(xfb[fb], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);
    
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_NRM, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0); // vertex
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0); // normals
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGB8, 0); // color
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0); // texture
    
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    
    GX_SetNumTexGens(1);
    GX_InvalidateTexAll();
    
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    
    //=================================================================
    
}

void swapBuffers (void)
{
    // Flip the framebuffer and flush the display
    fb ^= 1;
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(xfb[fb], GX_TRUE);
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void doTexturedRender (void)
{
    static int textureHandle = projM->initRenderToTexture();
    static int frame = 0;
    frame++;
    
    //...
    
}

int main(int argc, char **argv)
{    
    // Initalise the display (and setup GX)
    displayInit();
    
    // Initialise the attached controllers
    WPAD_Init();
    
    // Initialise the attached FAT devices
    fatInitDefault();

    // Build our projectM settings
    struct projectM::Settings config;
    config.meshX = 32;                          // Width of PerPixel equation mesh
    config.meshY = 24;                          // Height of PerPixel equation mesh
    config.textureSize = 512;                   // Size of internal rendering texture
    config.fps = 35;                            // Target frames per second
    config.windowWidth = rmode->fbWidth;        // Display width (in pixels)
    config.windowHeight = rmode->efbHeight;     // Display height (in pixels)
    config.smoothPresetDuration = 10;           // Preset transition time (in seconds)
    config.presetDuration = 15;                 // Preset durtation (in seconds)
    config.beatSensitivity = 5.0f;              // Lower to make hard cuts more frequent
    config.aspectCorrection = true;             // Custom shape aspect correction
    config.shuffleEnabled = true;               // Preset shuffling
    config.wiiLightEnabled = true;              // Pulse the disc slot light in time with the beat
    config.easterEgg = 0.0f;                    // ...
    config.defaultPresetName = "";              // Default preset name
    config.presetURL = "sd:/presets";           // Location of preset directory

    // NOTE: projectM expects that the video system (including GX) has been
    //       configured and setup prior to its instantiation.
    
    // Allocate and initialise projectM
    projM = new projectM(config);
    if (!projM)
        exit(0);
    
    while (1) {
        
        // Read the latest controller states
        WPAD_ScanPads();
        
        // Get the buttons which were pressed this frame
        u32 pressed = WPAD_ButtonsDown(0);
        
        // Quit if 'HOME' was pressed
        if (pressed & WPAD_BUTTON_HOME) break;

        // If projectM has been instantiated
        if (projM) {
            
            // Switch to textured rendering if '1' was pressed
            if ((pressed & WPAD_BUTTON_1) && !texturedRender) texturedRender = true;
            
            // Generate some fake audio input
            // TODO: Add real audio?!
            fakePCM[0] = 0;
            for (int i = 1; i < MAX_AUDIO_SAMPLES; i++) {
                fakePCM[i] = fakePCM[i - 1] + (rand() % 200 - 100) * .002;
            }
            
            // Queue our fake audio input for beat analysis
            projM->pcm()->addPCMfloat(fakePCM, MAX_AUDIO_SAMPLES);

            // Render the next frame in the visualisation/preset
            projM->renderFrame();
            
            // If we are rendering to a texture then do that now
            if (texturedRender)
                doTexturedRender();
            
        }
        
        // Flip the framebuffer and flush the display
        swapBuffers();
        
    }

    // Cleanup
    if (projM)
        free(projM);
    
    // We return to the launcher application via exit
    exit(0);
    
    return 0;
}
