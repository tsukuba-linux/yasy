CC       = gcc
CFLAGS   = -O0 -g
INCLUDES = -I/usr/include/ffmpeg -I/usr/include/ffmpeg/libavcodec -I/usr/include/ffmpeg/libavformat -I/usr/include/ffmpeg/libswscale
LDLIBDIR = -L/usr/lib
LDLIBS   = -lavutil -lavformat -lavcodec -lavdevice -lswscale -lz -lm

yasy: yasy.c
	$(CC) $(CFLAGS) $(INCLUDES) $(LDLIBDIR) -o bin/yasy "yasy.c" $(LDLIBS)

clean:
	rm -rf *.o *.ko *.mod.o *.mod.c *~
	rm bin/*
	rm out/*

