MPlayer_CE,
unofficial Edition v.0.2 13.02.2009 

MPlayer Wii port © 2008/09 Team Twiizers,
used code © MPlayerWii[rOn], GeeXboX,
Scip, tipolosko, rodries, AgentX, 
DJDynamite123, Tantric, etc...
Play Files from DATA - DVD SDHC USB SMB
& Radio...    !!!***Enjoy***!!!

MPlayer_CE has turned into a extremely powerful media player.
Can play files from 
SD-HC
USB
DVD video & DATA DVD
SMB Samba
RADIO

Easy setup on Windows XP/Vista (Linux users, you require latest SMB instalation etc) 
just keep trying, windows is more user friendly

Create a folder on a root of any partition hard drive....
e.g. 
C:\Movies
right click the folder, click share & security, click SHARE ON NETWORK ok & apply!!!
Once that is done go into smb.conf (fill in the smb.conf like my e.g. below, 
if you DONT HAVE A PASSWORD, put a 0 like mine...... pass=0
it should look like this....

ip=192.168.0.206
share=Movies
user=MyNameIsDynamite!
pass=0

Radio, if you want to add more Radio streams, they need to end in .pls otherwise they crash MPlayer...
go in menu.conf and locate the Radio Streams, add yours below or above them in the SAME FORMAT!!!

DONT DELETE ANY OF THESE FILES, otherwise MPLAYER WILL NOT WORK AS INTENDED!

MPlayer has had many bugs fixed, it now includes aspect ratio, true dvd speed cache, USB fixes, auto-chain
(therefore, it can be used as a channel/legal of course!!! BUT still as always requires the mplayer_ce files on SD
!!!!!!!!!USE AT YOUR OWN RISK!!!!!!!!

!!!customisation!!! 
check my loop.avi I hope you use this one, but you can change it...*Hint Hint*

!Video/aspect!
MPlayerCE now has AutoAspect Original...
If you feel you need to autoboot mplayer to 16/9 or 2:35 then add a line like ratio=1.7778  to mplayer.conf
YOU CAN STILL USE IN MENU ASPECT OPTIONS :)
Also the original loop.avi shows as 4/3 this is due to MPlayer, to have a FULLSCREEN loop.avi
as i said above type in a line like ratio=1.7778 in mplayer.conf
this will auto to 16/9 and show loop.avi full screen with no cuts.....and menu layout is good aswell.

From all of us who have helped fix/test this player......Enjoy :) djdynamite123