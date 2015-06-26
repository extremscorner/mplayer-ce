### 0.76 - November 26, 2009 ###

  * Fixes for 4.2 update
  * Stability fixes (hopefully freezing is gone forever now)
  * Network (LAN and WAN) speeds increased
  * Added directory play feature
  * YouTube section entirely revamped
  * Added Nintendo Channel content (quality may not yet be optimal) (thanks to yellowstar6 for his work on ninchdl-listext)
  * `[`blip.tv was planned but couldn't make it on time`]`
  * Many, many more bugs fixed

### 0.75 - August 19, 2009 ###

  * FTP support added (thanks to hax)
  * NTFS support added for USB and SD (thanks to Shareese)
  * SMB speed improved
  * Hopefully finally eradicated the freezing bug

### 0.72 - August 3, 2009 ###

  * Fix playlist bug
  * More SMB servers detected (thanks to hax)
  * SMB now supports user-level share security (thanks to hax)
  * SMB now supports plain text passwords (thanks to hax)

### 0.71 - August 2, 2009 ###

  * Autoload implemented. If the next file has a very similar name (using a Levenshtein distance of 2) it will be autoloaded. For example:
    * film.part1.avi > film.part2.avi (1 change)
    * episode.s01e09.avi > episode.s01e10.avi works (2 changes)
  * Improved memory management: fixes MPlayer hanging if out of memory.
  * OSD level 3 now shows mem1 and mem2 info.
  * Driver improved with autodetection error: on error the device is reinitied and read is retried.
  * Restore Points fixed. Now quit or power off saves restore point and can resume properly. Please delete your previous restore\_point file in case it is corrupted, and check that your SD card isn't locked.
  * Improved SMB connections: now SMB will always reconnect.
  * Improved stream initialisation.
  * Fixed green lines at the top of loading screen.
  * Further improved video compatibility
  * Added ability to load an external driver from SD for USB 2.0 testing.  More details (and drivers) will be added to the Google Code page at a later date.  Thanks to Hermes for the patch.
  * Finally fixed the problem with network and USB 2.0 conflicts (due to the driver using a bad device).
  * Font loading improved.
  * Improved MPlayer support when a file unexpectedly disappears and reappears.
  * Many other small fixes and code cleanup.
  * New USB test:
    * If http://mplayer-ce.googlecode.com/files/USB%20Test.rar says USB device is compatible but a device is not working then there is no FAT partition on the device.  Check if you are using NTFS or exFat instead.

### 0.7 - July 9, 2009 ###

  * Use small cache if opening an internet stream (audio and video).
  * Optimized memory access (thanks to a suggestion from Shagkur); cache thread is now more stable.
  * A8 patch added for DVD playback in cIOS202 to avoid problems with old modchips.
  * Improved Libfat:
    * fix for special characters
    * prevent possible corruption on sd
    * can now mount any FAT partition, whether primary or active
    * real FAT32 limit so larger files can be played
  * Improved USB hotplug and DVD detection
  * ECHI module modified for greater compatibility with USB devices.
  * Improved video compatibility.
  * New buffering system implemented.  If the cache drops below 3% MPlayer will pause and re-buffer to prevent possible hangs with bad connections. Cache is shown on OSD level 3.
  * Shoutcast TV is complete with caching, thanks to Extrems.  Please see menu.conf.
  * If new cIOS is used then DVDx is not required for DVD access.
  * New cIOS Installer:
    * Installs IOS202 using base IOS60 automatically for greater WiFi support.
    * Ability to select the IOS to use so you can select an IOS with the fakesign bug.

### 0.62 - June 3, 2009 ###

  * USB LAN Adaptor support fixed (again).  Thanks to CountZ3ro for testing. Please note that you will need to install the USB 2.0 cIOS.
  * cIOS improved to stop conflicts with other homebrew. Please note that now only port0 has usb2 support. All other USB devices (including the USB LAN adaptor) must go in port1.  See here for details: `http://mplayer-ce.googlecode.com/files/usb.jpg`
  * Horizontal stretch parameter added (see mplayer.conf)
  * YouTube options added to menu.conf (thanks to Extrems)
  * Many small bug fixes

### 0.61 - June 3, 2009 ###

  * New screen size variables actually work now.
  * Better usb device detection
  * Using free Liberation font instead of Arial
  * Subtitle wraparound bug fixed
  * Released Spanish Edition

### 0.6 - May 23, 2009 ###

  * No more maximum cache limit
  * Introduced new variables into mplayer.conf to adjust screen size and position, please see mplayer.conf for details (component-fix is now deprecated)
  * idx/sub subtitle support (please note that these can take up to 30 seconds to load so please be patient)
  * Multiple folder locations added, it is now possible to have the files in
    * sd:/apps/mplayer\_ce
    * sd:/mplayer
    * usb:/apps/mplayer\_ce
    * usb:/mplayer
  * Added resume points - video will resume at last stopped point. To clear, delete resume\_points file in your mplayer\_ce folder. To seek to the beginning of the video hold 2 and press the minus button.
  * Added support for Hermes' cIOS.  This has greater USB compatibility and enables USB LAN connector support.  Please see wiki page for details
  * Added Fribidi library support for right-to-left languages
  * Made cache fill visible on screen
  * Many small bug fixes
  * Updated to latest MPlayer svn

### 0.5 - April 22, 2009 ###

  * SMB now much more robust - thanks to DennisLKJ for smb.c fix
  * libfat cache improved for speed and stability
  * USB ethernet now works with USB 1.1; USB 2.0 fix requires an updated cIOS
  * Added ability to manipulate picture using the nunchuck
  * Updated to latest MPlayer revision
  * Now using subfont.ttf instead of font folder - use mplayer.conf to change font size
  * Merged widescreen and 4:3 pack - now MPlayer will use the appropriate loop.avi automatically
  * Improved Modchip compatibility for DVD

### 0.4 - March 28, 2009 ###

  * USB 2.0 support (see docs for info)
  * TTF font support (see docs for info)
  * Fixed bug in radio streaming.
  * DVD-Video bugs fixed (DVDs should play much more smoothly now)
  * Fixes in Libdi to detect chipped Wii
  * Codec fixes
  * Updated to latest MPlayer revision
  * Many small fixes

### 0.3a - March 2, 2009 ###

  * Fixed a little bug in keepalive issue that hangs the wii

### 0.3 - February 27, 2009 ###

  * Fixed code to play videos with incompatible size; we now support many video formats and unorthodox resolutions
  * Fixed keepalive issue in samba (thanks to Ludovic Orban)
  * New loop.avi and widescreen version (thanks to Blue\_K)

### 0.21e - February 24, 2009 ###

  * Fixed apostrophies in filenames
  * Improved DVD and USB mounting devices
  * Fixed radio
  * Improved samba reconnection
  * Debugging help for SMB Shares at bootup, debug\_network=yes (Review mplayer.conf)

### 0.21d - February 22, 2009 ###

  * Updated menu.conf to allow selecting playlist
  * Fix rodries' loop patch

### 0.21c - February 21, 2009 ###

  * Updated menu.conf to allow selecting subtitles

### 0.21b - February 21, 2009 ###

  * New mplayer.conf option: component\_fix=yes to fix side bars on some problematic TVs (Now fixed) Review your configs.
  * Stop Looping Video/Audio file.
  * Fixed hang when you access dvdnav

### 0.21 - February 20, 2009 ###

  * Reduced font size
  * New Readme
  * New mplayer.conf option: component\_fix=yes to fix side bars on some problematic TVs

### 0.2 - February 17, 2009 ###

  * SD/USB Mount bugs fixed
  * DVD Mount/Stop/Motor/Pause problems fixed, now mounts, only when you select DVD
  * Network Initialising on startup changed, now connects "hidden" in background.
  * Added 5 SMB Shares, review smb.conf
  * Boot-up speed increased
  * Added files filter to only show audio/video files
  * Same directory kept open when you open/close the menu
  * Menu closes on file load

### 0.1 - February 14, 2009 ###

  * SMB fixes
  * DVD cache bug fixed
  * Small USB fixes
  * Modified Libogc & Libfat
  * 2.35:1 videos now scale correctly