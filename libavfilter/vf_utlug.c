/*
 * utlug.c - Video filter for University of Tsukuba Linux User Group study sessions.
 * 
 * This filter is based on vf_drawbox.c which is an example contained libavfilter.
 * Many thanks!
 *
 */

// These libraries will 
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Using GD libraries
#include <gd.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// Using libavfilter and some utility libraries
#include "avfilter.h"
#include "parseutils.h"
#include "libavcodec/colorspace.h"

#define RGB2Y(r,g,b) (  0.299 * r + 0.587 * g + 0.114 * b)
#define RGB2U(r,g,b) ( -0.169 * r - 0.332 * g + 0.5   * b + 128)
#define RGB2V(r,g,b) (  0.5   * r - 0.419 * g - 0.081 * b + 128)

typedef struct
{
  int x;
  int y;
  int angle;
} UtlugTransform;

typedef struct
{
  int x, y, w, h;
  int vsub, hsub;   //< chroma subsampling
  char title[255];
  char name[255];
  gdImage** loaded_images;
  int num_images;
} UtlugContext;

static av_cold int init(AVFilterContext *ctx, const char *args, void *opaque)
{
  UtlugContext *context= ctx->priv;
  char lua_filename[1024];
  char lua_error[1024];
  char test[1024];
  FILE* file;
  lua_State* L;

  if(!args || strlen(args) > 1024) {
    av_log(ctx, AV_LOG_ERROR, "Invalid arguments!\n");
    return -1;
  }

  sscanf(args, "%s", lua_filename);
  av_log( ctx, AV_LOG_DEBUG, lua_filename);

  L = luaL_newstate();
  luaL_openlibs(L);

  if( luaL_loadfile(L, lua_filename) || lua_pcall(L, 0, 0, 0) ) {
    snprintf( lua_error,"error : %s\n", lua_tostring(L, -1) );

    av_log(ctx, AV_LOG_ERROR, "Luaファイルを開けませんでした\n");
    av_log(ctx, AV_LOG_ERROR, lua_error);
    return 1;
  }


  lua_getglobal(L, "title");
  lua_getglobal(L, "name");
  strcpy( context->name,  lua_tostring(L, -1));
  strcpy( context->title, lua_tostring(L, -2));
  lua_pop(L, 2);

  lua_getglobal(L, "objects");
  lua_pushnil(L);  
  while (lua_next(L, -2) != 0) {  
    lua_pushvalue(L, -2);
    av_log(ctx, AV_LOG_ERROR, "%s, %s\n", lua_tostring(L, -2), lua_tostring(L, -1));
    lua_pop(L, 2);
  }
  lua_pop(L, 1);  
  
  av_log(ctx, AV_LOG_ERROR, "%s\n", context->name);
  av_log(ctx, AV_LOG_ERROR, "%s\n", context->title);

  context->num_images = 1;
  context->loaded_images = (gdImage**)av_malloc( sizeof(gdImage*));
  file = fopen( "headline.png", "rb");
  context->loaded_images[0] = gdImageCreateFromPng( file);
  fclose( file);
  
  lua_close(L);

  return 0;
}

static av_cold int uninit(AVFilterContext *ctx)
{
  UtlugContext *context= ctx->priv;
  int i;

  for( int i = 0; i < context->num_images; i++){
    gdImageDestroy(context->loaded_images[i]);
  }
}


static int query_formats(AVFilterContext *ctx)
{
  enum PixelFormat pix_fmts[] = {
    PIX_FMT_YUV444P,  PIX_FMT_YUV422P,  PIX_FMT_YUV420P,
    PIX_FMT_YUV411P,  PIX_FMT_YUV410P,
    PIX_FMT_YUVJ444P, PIX_FMT_YUVJ422P, PIX_FMT_YUVJ420P,
    PIX_FMT_YUV440P,  PIX_FMT_YUVJ440P,
    PIX_FMT_NONE
  };
  
  avfilter_set_common_formats(ctx, avfilter_make_format_list(pix_fmts));
  return 0;
}

static int config_input(AVFilterLink *link)
{
  UtlugContext *context = link->dst->priv;
  avcodec_get_chroma_sub_sample(link->format, &context->hsub, &context->vsub);
  return 0;
}

static AVFilterPicRef *get_video_buffer(AVFilterLink *link, int perms, int w, int h)
{
  return avfilter_get_video_buffer(link->dst->outputs[0], perms, w, h);
}

static void start_frame(AVFilterLink *link, AVFilterPicRef *picref)
{  
  avfilter_start_frame(link->dst->outputs[0], picref);
}

static void overlay(AVFilterPicRef *pic, UtlugContext* context, gdImage* img, UtlugTransform* trans)
{
  int x, y;
  int width, height;
  int channel;
  int angle;
  unsigned char* row[4];
  unsigned char* cy;
  unsigned char* cu;
  unsigned char* cv;
  gdImage* rimg;
  
  angle = trans->angle;
  width  = gdImageSX( img);
  height = gdImageSY( img);

  if( angle){
    rimg = gdImageCreate( width * 4, height * 4);
    gdImageCopyRotated( rimg, img,
			width >> 1, height >> 1,
			0, 0,
			width,
			height,
			angle % 360);
    width  = gdImageSX( rimg);
    height = gdImageSY( rimg);
  }else{
    rimg = img;
  }

  for (y = 0; y < pic->h; y++) {
    row[0] = pic->data[0] + y * pic->linesize[0];
    for (channel = 1; channel < 3; channel++){
      row[channel] = pic->data[channel] + pic->linesize[channel] * (y >> context->vsub);
    }
    for (x = 0; x < pic->w; x++){
      if(( x > trans->x) && ( y > trans->y)){
	int c_index = gdImageGetPixel( rimg, x - trans->x, y - trans->y);
	int r = gdImageRed(   rimg, c_index);
	int g = gdImageGreen( rimg, c_index);
	int b = gdImageBlue(  rimg, c_index);
	double a = (double)( 127 - gdImageAlpha( rimg, c_index)) / 127.0;
	
	cy = row[0] + x;
	cu = row[1] + (x >> context->hsub);
	cv = row[2] + (x >> context->hsub);
	
	if(( x < width + trans->x) && ( y < height + trans->y)){
	  *cy = (double)(*cy) * (1.0 - a) + (double)RGB2Y(r,g,b) * a;
	  *cu = (double)(*cu) * (1.0 - a) + (double)RGB2U(r,g,b) * a;
	  *cv = (double)(*cv) * (1.0 - a) + (double)RGB2V(r,g,b) * a;
	}
      }
    }
  }

  if(angle) gdImageDestroy( rimg);

}

void put_string( AVFilterPicRef* pic, UtlugContext* context, char*s,
		 int color, int bgcolor, int size,
		 char* font, UtlugTransform* transform)
{
  gdImage* img;
  int brect[8];

  int r = (color & 0xff000000) >> 24;
  int g = (color & 0x00ff0000) >> 16;
  int b = (color & 0x0000ff00) >> 8;
  int a = 127 - ((color & 0x000000ff) >> 1);

  int br = (bgcolor & 0xff000000) >> 24;
  int bg = (bgcolor & 0x00ff0000) >> 16;
  int bb = (bgcolor & 0x0000ff00) >> 8;
  int ba = 127 - ((bgcolor & 0x000000ff) >> 1);

  int x, y;
  int margin = size / 10;
  int total_margin = margin << 1;

  gdImageStringFT( NULL, &brect[0], 0,
		   font, size, 0.0, 0, 0, s);

  img = gdImageCreate( brect[2] - brect[6] + total_margin,
		       brect[3] - brect[7] + total_margin);

  gdImageColorResolve(img, br, bg, bb);

  gdImageStringFT( img, &brect[0],
		   gdImageColorResolveAlpha(img, r, g, b, a),
		   font, size, 0.0, margin, margin - brect[1] - brect[7], s);

  overlay( pic, context, img, transform);
  gdImageDestroy( img);

}


static void end_frame(AVFilterLink *link)
{
  static int i = 0;
  UtlugTransform tr_headline = { 10, 10, i++};
  UtlugTransform tr_title = {10, 0, 0};
  UtlugContext *context = link->dst->priv;
  AVFilterLink *output = link->dst->outputs[0];
  AVFilterPicRef *pic = link->cur_pic;
  
  overlay(pic, context, context->loaded_images[0], &tr_headline);

  put_string(pic, context, "USBからLinuxを起動してみよう！ / うぶんちゅ! ",
	     0xffffffff, 0x653cc1ff,
	     10, "/usr/share/fonts/ipa-pgothic/ipagp.otf", &tr_title);

  avfilter_draw_slice(output, 0, pic->h);
  avfilter_end_frame(output);
}

AVFilter avfilter_vf_drawbox=
{
    .name      = "utlug",
    .priv_size = sizeof(UtlugContext),
    .init      = init,
    .uninit    = uninit,

    .query_formats   = query_formats,
    .inputs    = (AVFilterPad[]) {{ .name            = "default",
                                    .type            = CODEC_TYPE_VIDEO,
                                    .get_video_buffer= get_video_buffer,
                                    .start_frame     = start_frame,
                                    .end_frame       = end_frame,
                                    .config_props    = config_input,
                                    .min_perms       = AV_PERM_WRITE |
                                                       AV_PERM_READ,
                                    .rej_perms       = AV_PERM_REUSE |
                                                       AV_PERM_REUSE2},
                                  { .name = NULL}},
    .outputs   = (AVFilterPad[]) {{ .name            = "default",
                                    .type            = CODEC_TYPE_VIDEO, },
                                  { .name = NULL}},
};
