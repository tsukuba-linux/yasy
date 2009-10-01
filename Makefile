CC       = gcc
CFLAGS   = -O2
INCLUDES = -I/usr/include/ffmpeg
LDLIBDIR = -L/usr/lib
LDLIBS   = -lavutil -lavformat -lavcodec -lavdevice -lswscale -lz -lm

hello: yasy.c
	$(CC) $(CFLAGS) `pkg-config opencv --cflags` `pkg-config opencv --libs` $(INCLUDES) $(LDLIBDIR) -o bin/yasy "yasy.c" $(LDLIBS)

clean:
	rm -rf *.o *.ko *.mod.o *.mod.c *~
	rm bin/*
	rm out/*

