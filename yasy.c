#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>

#define DEBUG


AVFrame *picture, *tmp_picture;
uint8_t *video_outbuf;
int frame_count, video_outbuf_size;

float t, tincr, tincr2;
int16_t *samples;
uint8_t *audio_outbuf;
int audio_outbuf_size;
int audio_input_frame_size;

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
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

static void get_audio_frame(int16_t *samples, int frame_size, int nb_channels)
{
    int j, i, v;
    int16_t *q;

    q = samples;
    for(j=0;j<frame_size;j++) {
        v = (int)(sin(t) * 10000);
        for(i = 0; i < nb_channels; i++)
            *q++ = v;
        t += tincr;
        tincr += tincr2;
    }
}

static void write_audio_frame(AVFormatContext *oc, AVStream *st)
{
    AVCodecContext *c;
    AVPacket pkt;
    av_init_packet(&pkt);

    c = st->codec;

    get_audio_frame(samples, audio_input_frame_size, c->channels);

    pkt.size= avcodec_encode_audio(c, audio_outbuf, audio_outbuf_size, samples);

    if (c->coded_frame->pts != AV_NOPTS_VALUE)
        pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
    pkt.flags |= PKT_FLAG_KEY;
    pkt.stream_index= st->index;
    pkt.data= audio_outbuf;

    /* write the compressed frame in the media file */
    if (av_interleaved_write_frame(oc, &pkt) != 0) {
        fprintf(stderr, "Error while writing audio frame\n");
        exit(1);
    }
}

void open_audio(AVFormatContext *oc, AVStream *st){
    AVCodecContext *c;
    AVCodec *codec;

    c = st->codec;

    /* find the audio encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    /* open it */
    if (avcodec_open(c, codec) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }

    /* init signal generator */
    t = 0;
    tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

    audio_outbuf_size = 10000;
    audio_outbuf = av_malloc(audio_outbuf_size);

    /* ugly hack for PCM codecs (will be removed ASAP with new PCM
       support to compute the input frame size in samples */
    if (c->frame_size <= 1) {
        audio_input_frame_size = audio_outbuf_size / c->channels;
        switch(st->codec->codec_id) {
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U16BE:
            audio_input_frame_size >>= 1;
            break;
        default:
            break;
        }
    } else {
        audio_input_frame_size = c->frame_size;
    }
    samples = av_malloc(audio_input_frame_size * 2 * c->channels);
}


AVStream *add_audio_stream(AVFormatContext *oc, enum CodecID codec_id){

    AVCodecContext *c;
    AVStream *st;

    st = av_new_stream(oc, 1);

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = CODEC_TYPE_AUDIO;

    /* put sample parameters */
    c->bit_rate = 64000;
    c->sample_rate = 44100;
    c->channels = 2;
    return st;
}

static void close_audio(AVFormatContext *oc, AVStream *st)
{
    avcodec_close(st->codec);

    av_free(samples);
    av_free(audio_outbuf);
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
  AVCodec *out_codec;
  AVStream *out_stream_audio;
  AVStream *out_stream_video;
  double out_audio_pts;
  double out_video_pts;
  
  // Structures to read
  AVInputFormat  *in_fmt;
  AVFormatContext *in_fmt_ctx;
  AVCodecContext *in_cdc_ctx;
  AVCodec *in_cdc;
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
  
  int i;
 
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

  in_cdc_ctx = in_fmt_ctx->streams[in_stream_video]->codec;
  in_cdc = avcodec_find_decoder( in_cdc_ctx->codec_id);

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

  avcodec_open( in_cdc_ctx, in_cdc);

  // add out stream
  out_stream_video = av_new_stream( out_fmt_ctx, 0);
  enum CodecID codec_id = av_guess_codec( out_fmt_ctx->oformat,
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


  if ( out_fmt->audio_codec != CODEC_ID_NONE) {
    out_stream_audio = add_audio_stream( out_fmt_ctx, out_fmt->audio_codec);
  }
  av_set_parameters( out_fmt_ctx, NULL);

  dump_format( out_fmt_ctx, 0, o_filename, 1);


  int ret = avcodec_open( out_cdc_ctx, out_codec);
  if(ret != 0) error("can't open codec(encoder).");

  if (! ( out_fmt->flags & AVFMT_NOFILE )) {
    ret = url_fopen(&out_fmt_ctx->pb, out_fmt_ctx->filename, URL_WRONLY);
    if (ret < 0) error("can't open output file.");
  }
  

  if(out_stream_audio) open_audio( out_fmt_ctx, out_stream_audio);

  av_write_header( out_fmt_ctx);

  frame = avcodec_alloc_frame();
  frame_rgb = avcodec_alloc_frame();

  int buf_size = out_cdc_ctx->width * out_cdc_ctx->height * 4;
  uint8_t *buf = av_malloc(buf_size);
  

  out_cdc_ctx->bit_rate = 128000;
  out_cdc_ctx->qcompress = 0.0;
  out_cdc_ctx->qmin = 0;
  out_cdc_ctx->qmax = 2;

  i = 0;
  while( av_read_frame( in_fmt_ctx, &packet) >= 0){
    int x;
    int y;
    // Check if this packet is from video stream
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

	for( y = 0; y < in_cdc_ctx->height; y++){
	  for( x = 0; x < in_cdc_ctx->width; x++){
	    int p = x * 3 + y * frame_rgb->linesize[0];
	    frame_rgb->data[0][p]   = 0xff;
	    //pFrameRGB->data[0][p+1] = 0xff;
	    //pFrameRGB->data[0][p+2] = 0xff;
	  }
	}


        sws_scale( rgb2target,
		   frame_rgb->data,
		   frame_rgb->linesize,
		   0,
		   in_cdc_ctx->height,
		   frame->data,
		   frame->linesize);
        sws_freeContext( rgb2target);

	int out_size = avcodec_encode_video ( out_cdc_ctx, buf, buf_size, frame);
	SaveFrame( frame_rgb, out_cdc_ctx->width, out_cdc_ctx->height, i);
	printf( "bitrate: %i\n", out_cdc_ctx->bit_rate);
	printf( "qcomp  : %f\n", out_cdc_ctx->qcompress);
	printf( "qmin   : %i\n", out_cdc_ctx->qmin);
	printf( "qmax   : %i\n", out_cdc_ctx->qmax);
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
	if( i > 30) break;
      }
    }
    av_free_packet( &packet);
  }

  puts("Encode completed");
  av_write_trailer( out_fmt_ctx);
  puts("Trailer writing completed");  

  if (out_stream_audio) close_audio( out_fmt_ctx, out_stream_audio);

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
  av_close_input_file( in_fmt_ctx);
  av_free( out_fmt_ctx);
  puts("Allocated memories released");
  puts("ALL YOUR BASES ARE BELONG TO US");

  return 0;
  
}
