include $(SUBDIR)../config.mak

NAME = avfilter
FFLIBS = avcodec avutil 
FFLIBS-$(CONFIG_SCALE_FILTER)  += swscale
FFLIBS-$(CONFIG_AVFILTER_LAVF) += avformat

HEADERS = avfilter.h

OBJS = allfilters.o                                                     \
       avfilter.o                                                       \
       avfiltergraph.o                                                  \
       defaults.o                                                       \
       formats.o                                                        \
       graphparser.o                                                    \
       parseutils.o                                                     \


OBJS-$(CONFIG_UTLUG_FILTER)      += vf_utlug.o
OBJS-$(CONFIG_CROP_FILTER)       += vf_crop.o
OBJS-$(CONFIG_DRAWBOX_FILTER)    += vf_drawbox.o
OBJS-$(CONFIG_FPS_FILTER)        += vf_fps.o
OBJS-$(CONFIG_HFLIP_FILTER)      += vf_hflip.o
OBJS-$(CONFIG_NEGATE_FILTER)     += vf_negate.o
OBJS-$(CONFIG_FIFO_FILTER)       += vf_fifo.o
OBJS-$(CONFIG_FORMAT_FILTER)     += vf_format.o
OBJS-$(CONFIG_NULL_FILTER)       += vf_null.o
OBJS-$(CONFIG_OVERLAY_FILTER)    += vf_overlay.o
OBJS-$(CONFIG_ROTATE_FILTER)     += vf_rotate.o
OBJS-$(CONFIG_SCALE_FILTER)      += vf_scale.o
OBJS-$(CONFIG_SETPTS_FILTER)     += vf_setpts.o
OBJS-$(CONFIG_SLICIFY_FILTER)    += vf_slicify.o
OBJS-$(CONFIG_SPLIT_FILTER)      += vf_split.o
OBJS-$(CONFIG_TRANSPOSE_FILTER)  += vf_transpose.o
OBJS-$(CONFIG_VFLIP_FILTER)      += vf_vflip.o

OBJS-$(CONFIG_BUFFER_FILTER)     += vsrc_buffer.o
OBJS-$(CONFIG_MOVIE_FILTER)      += vsrc_movie.o

include $(SUBDIR)../subdir.mak
