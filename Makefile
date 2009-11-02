CC       = gcc
CFLAGS   = -g -O0 #`glib-config --cflags` `pkg-config opencv --cflags` `pkg-config opencv --libs`
INCLUDES = -I/usr/include/ffmpeg -I/usr/include/ffmpeg/libavcodec -I/usr/include/ffmpeg/libavformat -I/usr/include/ffmpeg/libswscale
LDLIBDIR = -L/usr/lib
LDLIBS   = -lavutil -lavformat -lavcodec -lavdevice -lswscale -lz -lm -lgd -lpng -ljpeg -lfreetype

yasy: yasy.c
	$(CC) $(CFLAGS) $(INCLUDES) $(LDLIBDIR) -o yasy "yasy.c" $(LDLIBS)

clean:
	rm -rf yasy *.o *.ko *.mod.o *.mod.c *~

