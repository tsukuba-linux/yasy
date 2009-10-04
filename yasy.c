
// Using sevral standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Using FFmpeg API libraries
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>

// Using OpenCV libraries
#include <cv.h>
#include <cxcore.h>
#include <highgui.h>

// Using GD libraries
#include <gd.h>

#define DEBUG

float t, tincr, tincr2;
int16_t *samples;
uint8_t *audio_outbuf;
int audio_outbuf_size;
int audio_input_frame_size;

void yasy_overlay( AVFrame* frame, gdImage* img,
		   int offset_x, int offset_y,
		   int video_width, int video_height)
{

  int x, y;

  int width  = gdImageSX( img);
  int height = gdImageSY( img);

  for( y = 0; y < height; y++){
    for( x = 0; x < width; x++){

      int r, g, b;
      int p = x * 3 + y * frame->linesize[0];
      int c_index = gdImageGetPixel( img, x, y);
      double a = (double)( 127 - gdImageAlpha( img, c_index)) / 127.0;

      r = frame->data[0][p  ] * (1.0 - a) + (double)gdImageRed(   img, c_index) * a;
      g = frame->data[0][p+1] * (1.0 - a) + (double)gdImageGreen( img, c_index) * a;
      b = frame->data[0][p+2] * (1.0 - a) + (double)gdImageBlue(  img, c_index) * a;
						  
      frame->data[0][p] = r;
      frame->data[0][p+1] = g;
      frame->data[0][p+2] = b;

    }
  }

}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  sprintf(szFilename, "out/frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  for(y=0; y<height; y++){
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  }
  fclose(pFile);
}

int main(int argc, char **argv)
{
  
  // IO filename
  const char *i_filename;
  const char *o_filename;
  
  // Structures to write 
  AVOutputFormat  *out_fmt;
  AVFormatContext *out_fmt_ctx;
  AVCodecContext *out_cdc_ctx;
  AVCodecContext *out_cdc_ctx_audio;
  AVCodec *out_codec;
  AVCodec *out_codec_audio;
  AVStream *out_stream_audio;
  AVStream *out_stream_video;
  double out_audio_pts;
  double out_video_pts;
  
  // Structures to read
  AVInputFormat  *in_fmt;
  AVFormatContext *in_fmt_ctx;
  AVCodecContext *in_cdc_ctx;
  AVCodecContext *in_cdc_ctx_audio;
  AVCodec *in_cdc;
  AVCodec *in_cdc_audio;
  int in_stream_audio;
  int in_stream_video;
  double in_audio_pts;
  double in_video_pts;

  int frame_finished;
  uint8_t* buffer;
  AVFrame *frame;
  AVFrame *frame_rgb;
  AVPacket packet;
  int num_bytes;


  AVRational framerate;
  int nb_frames;
  int bitrate;
  int width;
  int height;
  double audio_pts;
  double video_pts;
  
  gdImage* img;
  FILE* infile;
  int i;


  infile = fopen( "samples/headline_1-1.png", "rb");
  img = gdImageCreateFromPng( infile);
  fclose( infile);

  av_register_all();

  i_filename = argv[1];
  o_filename = argv[2];
#ifdef DEBUG
  printf( " Input: %s\n", i_filename);
  printf( "Output: %s\n", o_filename);
#endif
  
  /*
   * Guess {In/Out}put file format
   * Use mpeg if could not
   */
  in_fmt = (AVInputFormat*)guess_format(NULL, i_filename, NULL);
  if (!in_fmt) {
    printf("Could not guess input, attempt to use mpeg");
    in_fmt = (AVInputFormat*)guess_format("mpeg", NULL, NULL);
  }
  if (!in_fmt) {
    fprintf(stderr, "No formats to input were available\n");
    exit(1);
  }

  out_fmt = guess_format(NULL, o_filename, NULL);
  if (!out_fmt) {
    printf("Could not guess input, attempt to use mpeg");
    out_fmt = guess_format("mpeg", NULL, NULL);
  }
  if (!out_fmt) {
    fprintf(stderr, "No formats to input were available\n");
    exit(1);
  }
#ifdef DEBUG
  printf( " Input format: %s\n",  in_fmt->long_name);
  printf( "Output format: %s\n", out_fmt->long_name);
#endif

  /*
   * Allocation of media context(?)
   */
  
  out_fmt_ctx = av_alloc_format_context();//avformat_alloc_context();
  if (!out_fmt_ctx) {
    fprintf(stderr, "Memory error\n");
    exit(1);
  }
  out_fmt_ctx->oformat = out_fmt;
  snprintf( out_fmt_ctx->filename,
	    sizeof(out_fmt_ctx->filename),
	    "%s", o_filename);

  // Open video file
  if( av_open_input_file( &in_fmt_ctx, i_filename, NULL, 0, NULL) != 0){
    exit(1);
  }
  dump_format( in_fmt_ctx, 0, i_filename, 0);

  // Find video stream and get codec information
  in_stream_video = -1;
  for( i = 0; i < in_fmt_ctx->nb_streams; i++){
    switch( in_fmt_ctx->streams[i]->codec->codec_type){
    case CODEC_TYPE_VIDEO:
      in_stream_video = i;
      break;      
    case CODEC_TYPE_AUDIO:
      in_stream_audio = i;
      break;      
    }
  }

  // Get codec to decode video of input 
  in_cdc_ctx = in_fmt_ctx->streams[in_stream_video]->codec;
  in_cdc = avcodec_find_decoder( in_cdc_ctx->codec_id);

  // Get codec to decode audio of input 
  in_cdc_ctx_audio = in_fmt_ctx->streams[in_stream_audio]->codec;
  in_cdc_audio = avcodec_find_decoder( in_cdc_ctx_audio->codec_id);

  bitrate = in_cdc_ctx->bit_rate;
  width  = in_cdc_ctx->width;
  height = in_cdc_ctx->height;
  nb_frames = in_fmt_ctx->streams[in_stream_video]->nb_frames;
  framerate.den = in_fmt_ctx->streams[in_stream_video]->time_base.den;
  framerate.num = in_fmt_ctx->streams[in_stream_video]->time_base.num;
#ifdef DEBUG
  printf( " frames: %i\n", nb_frames);
  printf( "bitrate: %i\n", bitrate);
  printf( "  width: %i\n", width);
  printf( " height: %i\n", height);
  printf( "Frame-rate: %i/%i\n", framerate.den, framerate.num);
#endif

  // Get codecs for input video and audio
  avcodec_open( in_cdc_ctx, in_cdc);
  avcodec_open( in_cdc_ctx_audio, in_cdc_audio);

  enum CodecID codec_id;
  // add out video stream and configure
  out_stream_video = av_new_stream( out_fmt_ctx, 0);
  codec_id = av_guess_codec( out_fmt_ctx->oformat,
			     NULL,
			     out_fmt_ctx->filename,
			     NULL, 
			     CODEC_TYPE_VIDEO);
  out_codec = avcodec_find_encoder(codec_id);
  if(out_codec == NULL) error("can't find codec(encoder).");
  out_cdc_ctx = out_stream_video->codec;
  out_cdc_ctx->codec_id   = out_codec->id;
  out_cdc_ctx->codec_type = CODEC_TYPE_VIDEO;
  out_cdc_ctx->width  = width;
  out_cdc_ctx->height = height;
  out_cdc_ctx->time_base.num = framerate.num;
  out_cdc_ctx->time_base.den = framerate.den;
  out_cdc_ctx->pix_fmt = PIX_FMT_NONE;
  if( out_codec && out_codec->pix_fmts){
    const enum PixelFormat *p = out_codec->pix_fmts;
    while(*p++ != -1){
      if(*p == out_cdc_ctx->pix_fmt) break;
    }
    if(*p == -1) out_cdc_ctx->pix_fmt = out_codec->pix_fmts[0];
  }
  if( out_fmt->flags & AVFMT_GLOBALHEADER)
    out_cdc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
  /*
  // add out auido stream and configure
  out_stream_audio = av_new_stream( out_fmt_ctx, 0);
  
  codec_id = av_guess_codec( out_fmt_ctx->oformat,
			     NULL,
			     out_fmt_ctx->filename,
			     NULL, 
			     CODEC_TYPE_AUDIO);
  out_codec_audio = avcodec_find_encoder(codec_id);
  if(out_codec_audio == NULL) error("can't find codec(encoder).");
  out_cdc_ctx_audio = out_stream_audio->codec;
  out_cdc_ctx_audio->codec_id = out_codec_audio->id;
  out_cdc_ctx_audio->codec_type = CODEC_TYPE_AUDIO;
  out_cdc_ctx_audio->bit_rate    = in_cdc_ctx_audio->bit_rate;
  out_cdc_ctx_audio->sample_rate = in_cdc_ctx_audio->sample_rate;
  out_cdc_ctx_audio->channels = 2;
  out_cdc_ctx_audio->time_base.num = in_fmt_ctx->streams[in_stream_audio]->time_base.num;
  out_cdc_ctx_audio->time_base.den = in_fmt_ctx->streams[in_stream_audio]->time_base.den;
  if( out_fmt->flags & AVFMT_GLOBALHEADER)
    out_cdc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
  */
  av_set_parameters( out_fmt_ctx, NULL);
  
  dump_format( out_fmt_ctx, 0, o_filename, 1);
  
  int ret;
  ret = avcodec_open( out_cdc_ctx, out_codec);
  //ret = avcodec_open( out_cdc_ctx_audio, out_codec_audio);
  
  if (! ( out_fmt->flags & AVFMT_NOFILE )) {
    ret = url_fopen(&out_fmt_ctx->pb, out_fmt_ctx->filename, URL_WRONLY);
    if (ret < 0) error("can't open output file.");
  }

  av_write_header( out_fmt_ctx);

  frame = avcodec_alloc_frame();
  frame_rgb = avcodec_alloc_frame();

  int buf_size = out_cdc_ctx->width * out_cdc_ctx->height * 4;
  uint8_t* buf = av_malloc(buf_size);
  int a_buf_size = AVCODEC_MAX_AUDIO_FRAME_SIZE * 2;
  uint8_t* a_buf = av_malloc(a_buf_size);

  out_cdc_ctx->bit_rate = 128000;
  out_cdc_ctx->qcompress = 0.0;
  out_cdc_ctx->qmin = 0;
  out_cdc_ctx->qmax = 2;

  /*
  out_cdc_ctx_audio->bit_rate = in_cdc_ctx_audio->bit_rate;
  out_cdc_ctx_audio->sample_rate = in_cdc_ctx_audio->sample_rate;
  out_cdc_ctx_audio->channels    = in_cdc_ctx_audio->channels;
  out_cdc_ctx_audio->sample_fmt  = in_cdc_ctx_audio->sample_fmt;
  */

  i = 0;
  int x;
  int y;
  int audio_size = FF_INPUT_BUFFER_PADDING_SIZE + AVCODEC_MAX_AUDIO_FRAME_SIZE;
  int16_t* sample = (int16_t*)malloc( audio_size);

  while( av_read_frame( in_fmt_ctx, &packet) >= 0){
    
    /*
    // the packet is from the audio stream
    if( packet.stream_index == in_stream_audio){
      avcodec_decode_audio2( in_cdc_ctx_audio,
			     sample,
			     &audio_size,
			     packet.data,
			     packet.size);
      if( audio_size){
	int out_size = avcodec_encode_audio( out_cdc_ctx_audio, a_buf, a_buf_size, sample);
	printf( "%i\n", out_fmt_ctx->nb_streams);
	if (out_size == 0){
	  continue;
	} else if (out_size < 0){
	  error("can't encode frame.");
	}

	AVPacket packet;
	av_init_packet(&packet);

	packet.stream_index = out_stream_audio->index;
	packet.data = a_buf;
	packet.size = a_buf_size;
	packet.pts = av_rescale_q( out_cdc_ctx_audio->coded_frame->pts, 
				   out_cdc_ctx_audio->time_base,
				   out_stream_audio->time_base);

	if( out_cdc_ctx->coded_frame->key_frame)
	  packet.flags |= PKT_FLAG_KEY;

	int ret = av_interleaved_write_frame( out_fmt_ctx, &packet);
	if(ret != 0) error("can't write frame.");
	
	av_free_packet( &packet);
      }
    }
    */

    // the packet is from the video stream
    if( packet.stream_index == in_stream_video){

      avcodec_decode_video( in_cdc_ctx,
			    frame,
			    &frame_finished,
			    packet.data,
			    packet.size);

      if( frame_finished){
        struct SwsContext *target2rgb = sws_getContext( in_cdc_ctx->width,
							in_cdc_ctx->height,
							in_cdc_ctx->pix_fmt,
							in_cdc_ctx->width,
							in_cdc_ctx->height,
							PIX_FMT_RGB24,
							SWS_BICUBIC,
							NULL,
							NULL,
							NULL);
        struct SwsContext *rgb2target = sws_getContext( in_cdc_ctx->width,
							in_cdc_ctx->height,
							PIX_FMT_RGB24,
							in_cdc_ctx->width,
							in_cdc_ctx->height,
							in_cdc_ctx->pix_fmt,
							SWS_BICUBIC,
							NULL,
							NULL,
							NULL);
	
	num_bytes = avpicture_get_size( PIX_FMT_RGB24,
					in_cdc_ctx->width,
					in_cdc_ctx->height);
	buffer = (uint8_t*)av_malloc( num_bytes * sizeof(uint8_t));
	
	avpicture_fill( (AVPicture*)frame_rgb,
			buffer,
			PIX_FMT_RGB24,
			in_cdc_ctx->width,
			in_cdc_ctx->height);

        sws_scale( target2rgb,
		   frame->data,
		   frame->linesize,
		   0,
		   in_cdc_ctx->height,
		   frame_rgb->data,
		   frame_rgb->linesize);
        sws_freeContext( target2rgb);

	yasy_overlay( frame_rgb, img,
		      10, 10,
		      out_cdc_ctx->width,
		      out_cdc_ctx->height);

        sws_scale( rgb2target,
		   frame_rgb->data,
		   frame_rgb->linesize,
		   0,
		   in_cdc_ctx->height,
		   frame->data,
		   frame->linesize);
        sws_freeContext( rgb2target);

	int out_size = avcodec_encode_video ( out_cdc_ctx, buf, buf_size, frame);
	//SaveFrame( frame_rgb, out_cdc_ctx->width, out_cdc_ctx->height, i);
#ifdef DEBUG
	printf( "bitrate: %i\n", out_cdc_ctx->bit_rate);
	printf( "qcomp  : %f\n", out_cdc_ctx->qcompress);
	printf( "qmin   : %i\n", out_cdc_ctx->qmin);
	printf( "qmax   : %i\n", out_cdc_ctx->qmax);
#endif
	if (out_size == 0){
	  continue;
	} else if (out_size < 0){
	  error("can't encode frame.");
	}

	AVPacket packet;
	av_init_packet(&packet);

	packet.stream_index = out_stream_video->index;
	packet.data= buf;
	packet.size= out_size;

	packet.pts= av_rescale_q( out_cdc_ctx->coded_frame->pts, 
				  out_cdc_ctx->time_base,
				  out_stream_video->time_base);
	if( out_cdc_ctx->coded_frame->key_frame)
	  packet.flags |= PKT_FLAG_KEY;

	int ret = av_interleaved_write_frame( out_fmt_ctx, &packet);
	if(ret != 0) error("can't write frame.");

	av_free( buffer);
	av_free_packet( &packet);
	printf( "%i\n", i);
	i++;
	//	if( i >= 300)break;
      }
    }
    av_free_packet( &packet);
  }

  puts("Encode completed");
  av_write_trailer( out_fmt_ctx);
  puts("Trailer writing completed");  

  for(i = 0; i < out_fmt_ctx->nb_streams; i++) {
    av_freep(&out_fmt_ctx->streams[i]->codec);
    av_freep(&out_fmt_ctx->streams[i]);
  }
  puts("Streams closed");
  
  if (!(out_fmt->flags & AVFMT_NOFILE)) {
    url_fclose(out_fmt_ctx->pb);
  }
  puts("Files are closed");

  av_free( frame);
  av_free( frame_rgb);
  avcodec_close( in_cdc_ctx);
  avcodec_close( in_cdc_ctx_audio);
  av_close_input_file( in_fmt_ctx);
  av_free( out_fmt_ctx);
  gdImageDestroy( img);
  free( sample);
  puts("Allocated memories released");
  puts("ALL YOUR BASES ARE BELONG TO US");

  return 0;
  
}
