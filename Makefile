CC       = gcc
CFLAGS   = -O0 -g
INCLUDES = -I/usr/include/ffmpeg -I/usr/include/ffmpeg/libavcodec -I/usr/include/ffmpeg/libavformat -I/usr/include/ffmpeg/libswscale
LDLIBDIR = -L/usr/lib
LDLIBS   = -lavutil -lavformat -lavcodec -lavdevice -lswscale -lz -lm

yasy: yasy.c
	$(CC) $(CFLAGS) $(INCLUDES) $(LDLIBDIR) "yasy.c" $(LDLIBS)

oute: output-example.c
	$(CC) $(CFLAGS) $(INCLUDES) $(LDLIBDIR) "output-example.c" $(LDLIBS)

clean:
	rm -rf *.o *.ko *.mod.o *.mod.c *~
	rm bin/*
	rm out/*
