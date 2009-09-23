@echo off
del mplayer-ce.dol
rem del mplayerz3.dol
del mplayer-ce.elf
cd ..
make libmplayerwii.a
if not %errorlevel% == 0 goto error
 
cd wiigui
make

goto end
 
:error
cd wiigui
 
:end