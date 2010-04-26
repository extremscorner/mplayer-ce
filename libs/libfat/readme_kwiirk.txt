This is a modified version of libfat with read_ahead patches that shows how to adapt it to cios_usb2
link with this libfat and use 
fopen("usb2:/foo","r")
to access the usb at usb2 speed.

@todo:
directly allocate data buffers in mem2
