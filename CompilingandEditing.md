# Introduction #

This page will tell you how to compile the source, and where to make changes.

## Compiling ##
In order to compile mplayer\_ce you first need to build the necessary libs, stored in the libs directory.

Please note that libdvdread and libdvdnav are now included in the main MPlayer release.  Make may give you an error if you still have the libs in your libogc folder, delete them to avoid the error.

Build:
  * libogc with 'make wii' and 'make install'
  * libfat with 'make wii-release' and 'make ogc-install'
  * libfreetype with 'make' and 'make install'
  * libjpeg with 'make' and 'make install'
  * libiconv with 'make' and 'make install-lib'
  * libfribidi with 'make' and 'make install'

Now you can build mplayer\_ce with 'make'.
It should create mplayer.dol in the mplayer-trunk directory.

## Editing the source ##
The files you will want to look at specific to the wii port are
```
/osdep/plat_gekko.h
/osdep/plat_gekko.c
/osdep/getch2-gekko.c
/osdep/gx_supp.c
/osdep/gx_supp.h
/libvo/vo_gekko.c
/libao/ao_gekko_.c
/libao/ao_gekko.c
mplayer.c
```
Several other files are also modified but not to a great extent.