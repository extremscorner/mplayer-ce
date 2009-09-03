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

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <projectM.hpp>

#include <stdlib.h>
#include <string.h>

#define SFX_BUFFER_SIZE         (8 * 1024)
#define SFX_BUFFERS             2

static lwp_t sfxThread;
static u8 sfxBuffer[SFX_BUFFERS][SFX_BUFFER_SIZE] ATTRIBUTE_ALIGN(32) = { { 0 } };
static u8 sfxSize[SFX_BUFFERS] = { 0 };
static u8 sfxWhichBuffer = 0;

static OggVorbis_File vf;
static char *audioFile = "sd:/audio/Karsten Koch - Leaving All Behind.ogg";
static bool playing = true;

static pm_config config;
static projectM *pm = NULL;
static char *presetDirectory = "sd:/presets";
static char *presetName = "Geiss - Cosmic Dust 2.milk";

void sfxSwitchBuffers()
{
    AUDIO_StopDMA();
    DCFlushRange(sfxBuffer[sfxWhichBuffer], sfxSize[sfxWhichBuffer]);
    AUDIO_InitDMA((u32) sfxBuffer[sfxWhichBuffer], sfxSize[sfxWhichBuffer]);
    AUDIO_StartDMA();
    
    sfxSize[sfxWhichBuffer] = 0;
    sfxWhichBuffer ^= 1;
}

void *sfxAudioThread(void *argv)
{
    int bitstream = 0;
    int samples = 0;
    
    // Open our audio file
    if (ov_fopen(audioFile, &vf) == 0)
        playing = true;
    
    // Audio loop
    while (playing) {
        
        //...
        samples = ov_read(&vf, (char *) sfxBuffer[sfxWhichBuffer], SFX_BUFFER_SIZE, 1, 1, 0, &bitstream);
        if (samples == 0) {
            playing = false;
        } else {
            sfxSize[sfxWhichBuffer] = samples;
        }
        
        // Queue our audio input for beat analysis
        //pm->pcm()->addPCM8(sfxBuffer);

        //...
        if(samples) {
            sfxSwitchBuffers();
        }
        
    };
    
    // Close our audio file
    ov_clear(&vf);
    
    return NULL;
}

int main(int argc, char **argv)
{
    // Initialise the video sytem
    // TODO: This, once projectM display is sorted out
    
    // Initialise the audio sytem
    AUDIO_Init(NULL);
    AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
    AUDIO_RegisterDMACallback(sfxSwitchBuffers);
    
    // Initialise the attached controllers
    WPAD_Init();
    
    // Initialise the attached FAT devices
    fatInitDefault();

    // Build our projectM settings
    config.maxFPS = 35;                         // Maximum frames per second
    config.meshX = 32;                          // Width of PerPixel equation mesh
    config.meshY = 24;                          // Height of PerPixel equation mesh
    config.textureSize = 512;                   // Size of internal rendering texture
    config.windowWidth = 512;                   // Display width (in pixels)
    config.windowHeight = 512;                  // Display height (in pixels)
    config.smoothPresetDuration = 10;           // Preset transition time (in seconds)
    config.presetDuration = 15;                 // Preset durtation (in seconds)
    config.beatSensitivity = 10.0f;             // Lower to make hard cuts more frequent
    config.aspectCorrection = true;             // Custom shape aspect correction
    config.shufflePresets = true;               // Preset shuffling
    config.pulseWiiLight = true;                // Pulse the wii disc slot light in time with the beat
    config.pulseSource = PM_AC_BASS;            // The audio characteristic to use for pulsing
    config.presetDirectory = presetDirectory;   // Location of preset directory
    config.initialPresetName = presetName;      // Initial preset name
    
    // Initialise projectM
    pm = new projectM(config);
    if (!pm)
        exit(0);

    //...
    //LWP_CreateThread(&sfxThread, sfxAudioThread, NULL, NULL, 0, LWP_PRIO_HIGHEST);
    
    // Video loop
    while (pm) {
        
        // Read the latest controller states
        WPAD_ScanPads();
        
        // Get the buttons which were pressed this frame
        u32 pressed = WPAD_ButtonsDown(0);
        
        // Quit if 'HOME' was pressed
        if (pressed & WPAD_BUTTON_HOME)
            break;
        
        // Toggle the wii disc slot light pulsing if '1' was pressed
        if ((pressed & WPAD_BUTTON_1))
            pm->settings().pulseWiiLight = !pm->settings().pulseWiiLight;
        
        // Render the next frame in the visualisation/preset
        pm->renderFrame();
        
    }

    //...
    playing = false;
    
    // Destroy projectM
    if (pm)
        free(pm);

    // We return to the launcher application via exit
    exit(0);
    
    return 0;
}
