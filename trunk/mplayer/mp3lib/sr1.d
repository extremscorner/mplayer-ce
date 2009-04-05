mp3lib/sr1.d mp3lib/sr1.o: mp3lib/sr1.c mp3lib/mpg123.h config.h mp3lib/huffman.h \
  mp3lib/mp3.h mpbswap.h config.h libavutil/bswap.h config.h \
  libavutil/common.h libavutil/mem.h cpudetect.h libavutil/x86_cpu.h \
  mp_msg.h libvo/fastmemcpy.h config.h libavutil/common.h \
  libavutil/internal.h libavutil/timer.h mp3lib/tabinit.c mp3lib/layer2.c \
  mp3lib/l2tables.h mp3lib/layer3.c mp3lib/dct64.c mp3lib/dct36.c \
  mp3lib/dct12.c mp3lib/decod386.c mp3lib/layer1.c
tabinit.d tabinit.o: mp3lib/tabinit.c
mp3lib/layer2.d mp3lib/layer2.o: mp3lib/layer2.c mp3lib/l2tables.h
mp3lib/layer3.d mp3lib/layer3.o: mp3lib/layer3.c mp3lib/dct64.c mp3lib/dct36.c mp3lib/dct12.c \
  mp3lib/decod386.c config.h
dct64.d dct64.o: mp3lib/dct64.c
dct36.d dct36.o: mp3lib/dct36.c
dct12.d dct12.o: mp3lib/dct12.c
mp3lib/decod386.d mp3lib/decod386.o: mp3lib/decod386.c config.h
layer1.d layer1.o: mp3lib/layer1.c
