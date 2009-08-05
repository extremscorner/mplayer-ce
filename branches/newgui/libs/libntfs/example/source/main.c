/**
 * main.c - Directory listing example for NTFS-based devices.
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
#include <ntfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void list(const char *path)
{
    DIR *pdir;
    struct dirent *pent;
    struct stat st;
    
    // Open the directory
    pdir = opendir(path);
    if (pdir) {
        
        // List the contents of the directory
        while ((pent = readdir(pdir)) != NULL) {
            if ((strcmp(pent->d_name, ".") == 0) || (strcmp(pent->d_name, "..") == 0))
                continue;
            
            // Get the entries stats
            stat(pent->d_name, &st);
            //if (stat(pent->d_name, &st) == -1)
            //    continue;
            
            // List the entry
            if (S_ISBLK(st.st_mode)) {
                printf(" B %s\n", pent->d_name);
            } else if (S_ISCHR(st.st_mode)) {
                printf(" C %s\n", pent->d_name);
            } else if (S_ISDIR(st.st_mode)) {
                printf(" D %s\n", pent->d_name);
            } else if (S_ISFIFO(st.st_mode)) {
                printf(" P %s\n", pent->d_name);
            } else if (S_ISREG(st.st_mode)) {
                printf(" F %s\n", pent->d_name);
            } else if (S_ISLNK(st.st_mode)) {
                printf(" L %s\n", pent->d_name);
            } else {
                printf(" ? %s\n", pent->d_name);
            }
            
        }
        
        // Close the directory
        closedir(pdir);
        
    } else {
        printf("opendir(%s) failure.\n", path);
    }
    
    printf("\n");
    
    return;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
    
    // Initialise the video system
    VIDEO_Init();
    
    // This function initialises the attached controllers
    WPAD_Init();
    
    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);
    
    // Allocate memory for the display in the uncached region
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    
    // Initialise the console, required for printf
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    
    // Set up the video registers with the chosen mode
    VIDEO_Configure(rmode);
    
    // Tell the video hardware where our display memory is
    VIDEO_SetNextFramebuffer(xfb);
    
    // Make the display visible
    VIDEO_SetBlack(FALSE);
    
    // Flush the video register changes to the hardware
    VIDEO_Flush();
    
    // Wait for Video setup to complete
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
    
    
    // The console understands VT terminal escape codes
    // This positions the cursor on row 2, column 0
    // we can use variables for this with format codes too
    // e.g. printf ("\x1b[%d;%dH", row, column );
    printf("\x1b[2;0H");

    printf("\n");
    printf(" NTFS Directory Listing Example\n");
    printf("\n");
    printf(" - 'LEFT' and 'RIGHT' to select a volume\n");
    printf(" - 'A' to enumerate the selected volume\n");
    printf(" - 'HOME' to quit\n");
    printf("\n");

    bool listed = false;
    ntfs_md *mounts = NULL;
    int mountCount = 0;
    int mountIndex = 0;
    char path[MAX_PATH] = {0};
    int i;
    
    // Initialise and mount FAT devices
    //fatInitDefault();

    // Mount all NTFS volumes on all inserted block devices
    mountCount = ntfsMountAll(&mounts, NTFS_DEFAULT | NTFS_RECOVER);
    if (mountCount == -1)
        printf("Error whilst mounting devices.\n");
    else if (mountCount > 0)
        printf("%i NTFS volumes(s) found!\n\n", mountCount);
    else
        printf("No NTFS volumes were found.\n");
    
    // List all mounted NTFS volumes
    for (i = 0; i < mountCount; i++) {
        printf("%i - %s:/ (%s)\n", i + 1, mounts[i].name, ntfsGetVolumeName(mounts[i].name)); 
    }
    
    printf("\n");
    while (1) {
        
        // Call WPAD_ScanPads each loop, this reads the latest controller states
        WPAD_ScanPads();
        
        // WPAD_ButtonsDown tells us which buttons were pressed in this loop
        // this is a "one shot" state which will not fire again until the button has been released
        u32 pressed = WPAD_ButtonsDown(0);
        
        // Break from main loop
        if (pressed & WPAD_BUTTON_HOME) break;

        // If there is a volume to list and we have not yet listed one...
        if (mountCount > 0 && !listed) {

            // Deincrement the selected volumes index
            if (pressed & WPAD_BUTTON_LEFT)
                mountIndex = MIN(MAX(mountIndex - 1, 0), mountCount - 1);
            
            // Increment the selected volumes index
            if (pressed & WPAD_BUTTON_RIGHT)
                mountIndex = MIN(MAX(mountIndex + 1, 0), mountCount - 1);
            
            // Enumerate the selected volumes contents
            if (pressed & WPAD_BUTTON_A) {
                printf("\n\n");
                
                // Move to the volumes root directory
                strcpy(path, mounts[mountIndex].name);
                strcat(path, ":/");
                //chdir(path);
                
                // Enumerate the volumes contents
                list(path);
                
                sprintf(path, "%s:/test", mounts[mountIndex].name);
                struct stat st;
                if(stat(path, &st)) {
                    printf("Creating directory \"%s\"\n", path);
                    if (mkdir(path, S_IRWXU | S_IROTH | S_IXOTH)) {
                        printf("mkdir(%s) FAILED!\n", path);
                    }
                } else {
                    printf("Directory \"%s\" already exists, not creating\n", path);
                }

                printf("\n");
                strcat(path, "/text.txt");
                FILE *f = fopen(path, "r");
                if (f) {
                    char buf[1024] = {0};
                    
                    fseek(f, 0, SEEK_END);
                    ssize_t size = ftell(f);
                    printf("File Size: %d\n", size);
                    
                    fseek(f, 100, SEEK_SET);
                    fread(&buf, sizeof(char), 300, f);
                    printf("File Contents (pos: 100, len: 300):\n\n%s\n", buf);
                    
                    fclose(f);
                    
                } else {
                    printf("fopen(%s) FAILED!\n", path);
                }
                
                printf("\nPress 'HOME' to quit.\n\n");
                
                listed = true;
                
            }
            
            // If we have not listed a volume yet then prompt to select one
            if(!listed) {
                printf("\rSelect a NTFS volume: < %i >", mountIndex + 1);
            }
        
        }
        
        // Wait for the next frame
        VIDEO_WaitVSync();
        
    }

    // Unmount all NTFS volumes and clean up
    if (mounts) {
        for (i = 0; i < mountCount - 1; i++) {
            ntfsUnmount(mounts[0].name, true); 
        }
        free(mounts);
    }
    
    // We return to the launcher application via exit
    exit(0);
    
    return 0;
}
