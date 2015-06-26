# Introduction #

This page will explain how to set up your config files, including SMB shares, customising the menu layout in menu.conf, adding input with input.conf and changing default behaviour with mplayer.conf.

This page will also explain changing the loop.avi.

## Setup and Edit your Configs. ##

### smb.conf ###

First of all if you are planning on using SMB, be sure to follow the USB2.0 installation as the IOS60 as a base to install as IOS202 gives much better wifi stability. The Instalation can be found: [Here](http://code.google.com/p/mplayer-ce/wiki/USB20).

Your smb.conf has 5 shares.
Determine how many shares you want to use and fill in the fields.
For example, I have a folder shared on C:\ called Movies.  I don't have a password on my user account, and my folder isn't protected. Therefore I don't need to add my user in the field in the example below.  Typically shares made with Windows XP won't need the username or password fields filled in.

```
#Samba share1  (smb1:/) 
ip1=192.168.0.2
share1=Movies
user1= 
pass1=
```

I also have a folder on D:\ but my shared folder has a space: My Music.  Therefore, quotation marks are needed:

```
#Samba share2  (smb2:/)
ip2=192.168.0.2
share2="My Music"
user2=
pass2=
```

If you need a username and password to access shared folders, simply fill all fields in.

Windows 7 & Vista advanced settings:
Setting the NT Access rights on the shared folder to allow "Everyone" read and execute access.

Right-click folder, then pick "Security", "Advanced", "Change Permissions", "Add", "Advanced", "Find now", then pick "Everyone", then give read permissions.

**Tutorials for setting up a file share in Windows XP and Windows Vista are available.**

http://www.practicallynetworked.com/sharing/xp/filesharing.htm

http://cws.internet.com/article/3592-.htm


### menu.conf ###

You can change the layout of your menu.conf.
Doing so will change the main menu layout in MPlayerCE.

There is little existing documentation on editing menu.conf, but it is that not difficult to figure out by looking at an example.  It is an xml-like file and it takes the normal [MPlayer commands](http://www.mplayerhq.hu/DOCS/tech/slave.txt).  To use it to load a specific file or stream use loadfile, e.g. to load a SopCast stream from my own network:

```
<cmdlist name="browse" title="Browse" ptr="<>" >
    ...
    <e name="SopCast ..." ok="loadfile http://192.168.1.65:8902/"/>
</cmdlist>
```

**Remember** you can add Radio streams, look under Radio in menu.conf and use the same technique as the others.

### input.conf ###
The Wii Remote and Gamecube controller controls are mapped as:

```
GAMECUBE CONTROLLER     WII REMOTE          KEY
        A               A                   a
        B               B                   b
        X               1                   x
        Z               HOME                z
        L               MINUS               l
        R               PLUS                r
        LEFT            LEFT                LEFT
        RIGHT           RIGHT               RIGHT
        UP              UP                  UP
        DOWN            DOWN                DOWN
```

The Y button on the Gamecube Controller and the 2 button on the Wii Remote
acts as a modifier key (like shift) to add further control options:

```
GAMECUBE CONTROLLER     WII REMOTE          KEY
    Y + A               2 + A               A
    Y + B               2 + B               B
    Y + X               2 + 1               X
    Y + Z               2 + HOME            Z
    Y + L               2 + MINUS           L
    Y + R               2 + PLUS            R
    Y + LEFT            2 + LEFT            KP4
    Y + RIGHT           2 + RIGHT           KP6
    Y + UP              2 + UP              KP8
    Y + DOWN            2 + DOWN            KP2
```

To map a control in input.conf should be fairly self-explanatory.  First write the
key you wish to map and then the mplayer option you want, listed here: http://www.mplayerhq.hu/DOCS/tech/slave.txt
So for example, to make the OSD come up when you press 1 you would add
```
x osd
```
or to make 2+1 switch the ratio to 16:9 you would add
```
X switch_ratio 1.7778
```
### mplayer.conf ###

This is where you can add a line so MPlayerCE loads with a specific setting, for example forcing your aspect ratio always to 16:9.

You can add the line **aspect=1.7778** and MPlayerCE will default to 16/9.
The options are the standard [MPlayer options](http://www.mplayerhq.hu/DOCS/man/en/mplayer.1.html).
Change to suit you.

You can use the gxzoom, hor\_pos and vert\_pos parameters to adjust the screen size and position.  hor\_pos and vert\_pos offset the centre of the display and gxzoom adjusts the picture zoom. The lower the zoom value, the more the picture zooms. If you are experiencing black bars on the left and the right of your picture, try gxzoom=340.

# Customising #

### loop.avi ###

You can use your own **loop.avi**, just replace the current one with your own.
You can also have music in the loop.avi.  My loop.avi is 1 minute long and is 3.5mb in size with no issues.

If you are familiar with Virtualdub, you can use that to create your loop.avi. Simply editing the previous loop.avi by adding a new image or adding audio to it, etc.
You can use anything as your loop.avi aslong as it's xvid compression.

How to make static background loops for mplayer ce

Software used:
**Gimp**AVIDemux

1. Open Gimp and create a new image (640x456 for standard defenition, 624x352 for widescreen). You can use whatever image program you want, but Gimp is free, cross-platform, and powerful. So is avidemux, btw.

2. Edit the new image for your background so it looks how you want it. Save it as a jpg or png.

3. Open Avidemux. Set the video codec to xvid. Open the image you created. Copy the image and paste it several times so that you have a good number of frames. I'd say 60 frames is more than enough. Save the video as "loop.avi".

You're done.
Also, please submit your themes to http://code.google.com/p/mplayer-ce/wiki/UserSubmittedCustomizations

### Fonts ###

MPlayer supports freetype fonts.  Just place a ttf font in the mplayer\_ce directory named subfont.ttf.  You can use various arguments in mplayer.conf to adjust them:

```
subfont−autoscale <0−3>
Sets the autoscale mode.
NOTE: 0 means that text scale and OSD scale are font heights in points.
The mode can be:
0 no autoscale
1 proportional to movie height
2 proportional to movie width
3 proportional to movie diagonal (default)

subfont−blur <0−8>
Sets the font blur radius (default: 2).	

subfont−encoding <value>
Sets the font encoding. When set to ’unicode’, all the glyphs from the font file will be rendered and unicode will be used (default: unicode).	

subfont−osd−scale <0−100>
Sets the autoscale coefficient of the OSD elements (default: 6).	

subfont−outline <0−8>
Sets the font outline thickness (default: 2).	

subfont−text−scale <0−100>
Sets the subtitle text autoscale coefficient as percentage of the screen size (default: 5).
```

To enable fonts for your language there are some settings you can configure, thanks to mika7k7 for the guide.

```
1.Copy the Arial font from your Windows font folder (drag & drop) to mplayer_ce
directory on your SD, or acquire from somewhere else, taking copyright into
consideration.

2.Rename subfont.ttf to subfont.ttf.dist (as a backup)

3.Rename arial.ttf to subfont.ttf

4.Edit your mplayer.conf (w/ notepad) by adding to the end of the file:

#Subtitles Setting
slang=ar
subcp=windows-1256

5.That should do it.

Notes:
The font wii be tiny, edit

                subfont-text-scale=

in mplayer.conf to your liking.
```

You will need to change the slang and subcp parameters to fit your locale.