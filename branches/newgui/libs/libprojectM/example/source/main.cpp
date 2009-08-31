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

projectM *projM = NULL;
float fakePCM[MAX_AUDIO_SAMPLES] = { 0 };
bool texturedRender = false;

void doTexturedRender (void)
{
    static int frame = 0;
    frame++;
    
    //...
    
}

int main(int argc, char **argv)
{    
    // Initialise the attached FAT devices
    fatInitDefault();

    // Build our projectM settings
    struct projectM::Settings config;
    config.meshX = 32;                          // Width of PerPixel equation mesh
    config.meshY = 24;                          // Height of PerPixel equation mesh
    config.textureSize = 512;                   // Size of internal rendering texture
    config.fps = 35;                            // Target frames per second
    config.windowWidth = 512;                   // Display width (in pixels)
    config.windowHeight = 512;                  // Display height (in pixels)
    config.smoothPresetDuration = 10;           // Preset transition time (in seconds)
    config.presetDuration = 15;                 // Preset durtation (in seconds)
    config.beatSensitivity = 5.0f;              // Lower to make hard cuts more frequent
    config.aspectCorrection = true;             // Custom shape aspect correction
    config.shuffleEnabled = true;               // Preset shuffling
    config.wiiLightEnabled = true;              // Pulse the disc slot light in time with the beat
    config.easterEgg = 0.0f;                    // ...
    config.defaultPresetName = "";              // Default preset name
    config.presetURL = "sd:/presets";           // Location of preset directory

    // Allocate and initialise projectM
    projM = new projectM(config);
    if (!projM)
        exit(0);
    
    // Initialise the attached controllers
    WPAD_Init();
    
    while (projM) {
        
        // Read the latest controller states
        WPAD_ScanPads();
        
        // Get the buttons which were pressed this frame
        u32 pressed = WPAD_ButtonsDown(0);
        
        // Quit if 'HOME' was pressed
        if (pressed & WPAD_BUTTON_HOME) break;
        
        // Toggle wii disc slot light if '1' was pressed
        if ((pressed & WPAD_BUTTON_1)) { }
        
        // Switch to textured rendering if '2' was pressed
        if ((pressed & WPAD_BUTTON_2) && !texturedRender) texturedRender = true;
        
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

    // Cleanup
    if (projM)
        free(projM);
    
    // We return to the launcher application via exit
    exit(0);
    
    return 0;
}
