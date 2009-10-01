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

void write_video_frame(AVFormatContext *oc, AVStream *st, AVFrame* frame){
    int out_size, ret;
    AVCodecContext *c;
    static struct SwsContext *img_convert_ctx;

    c = st->codec;
    
    img_convert_ctx = sws_getContext( c->width,
				      c->height, 
				      PIX_FMT_RGB24,
				      c->width,
				      c->height,
				      c->pix_fmt,
				      SWS_BICUBIC, 
				      NULL, NULL, NULL);
    
    sws_scale( img_convert_ctx, frame->data, 
	       frame->linesize, 0, 
	       c->height,
	       picture->data, picture->linesize);
    
    
    puts("test6");
    
    if (oc->oformat->flags & AVFMT_RAWPICTURE) {
      AVPacket pkt;
      puts("test7");
      av_init_packet(&pkt);
      
      pkt.flags |= PKT_FLAG_KEY;
      pkt.stream_index= st->index;
      pkt.data= (uint8_t *)picture;
      pkt.size= sizeof(AVPicture);
      
      ret = av_interleaved_write_frame(oc, &pkt);
    } else {
      puts("test9");
      /* encode the image */
      out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture);
      puts("test10");
      /* if zero size, it means the image was buffered */
      if (out_size > 0) {
	AVPacket pkt;
	av_init_packet(&pkt);
	puts("test11");
	if (c->coded_frame->pts != AV_NOPTS_VALUE)
	  pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
	if(c->coded_frame->key_frame)
	  pkt.flags |= PKT_FLAG_KEY;
	pkt.stream_index= st->index;
	pkt.data= video_outbuf;
	pkt.size= out_size;
	puts("test12");
	ret = av_interleaved_write_frame(oc, &pkt);
	puts("test13");
      } else {
	ret = 0;
      }
    }
    puts("test8");
    if (ret != 0) {
      fprintf(stderr, "Error while writing video frame\n");
      exit(1);
    }
    frame_count++;
}

AVFrame *alloc_picture(enum PixelFormat pix_fmt, int width, int height){
    AVFrame *picture;
    uint8_t *picture_buf;
    int size;

    picture = avcodec_alloc_frame();
    if (!picture)
        return NULL;

    size = avpicture_get_size(pix_fmt, width, height);
    printf("GUDVSDVASDA: %i x %i = %i\n", width,height, size);
    picture_buf = av_malloc(size);
    if (!picture_buf) {
        av_free(picture);
        return NULL;
    }
    avpicture_fill((AVPicture *)picture, picture_buf,
                   pix_fmt, width, height);
    return picture;
}

void open_video(AVFormatContext *oc, AVStream *st){
    AVCodec *codec;
    AVCodecContext *c;

    c = st->codec;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    avcodec_open(c, codec);

    video_outbuf = NULL;
    if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
      puts("test14");
      video_outbuf_size = 200000;
      video_outbuf = av_malloc(video_outbuf_size);
    }
    
    /* allocate the encoded raw picture */
    picture = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!picture) {
        fprintf(stderr, "Could not allocate picture\n");
        exit(1);
    }

    /* if the output format is not YUV420P, then a temporary YUV420P
       picture is needed too. It is then converted to the required
       output format */
    tmp_picture = NULL;
    if (c->pix_fmt != PIX_FMT_YUV420P) {
        tmp_picture = alloc_picture(PIX_FMT_YUV420P, c->width, c->height);
        if (!tmp_picture) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
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

AVStream *add_video_stream( AVFormatContext* oc,
			    enum CodecID codec_id,
			    AVRational* fr,
			    int width, int height,
			    enum PixelFormat pixfmt){

    AVCodecContext *c;
    AVStream *st;

    st = av_new_stream(oc, 0);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = CODEC_TYPE_VIDEO;

    c->bit_rate = 400000;
    c->width = width;
    c->height = height;

    c->time_base.den = fr->den;
    c->time_base.num = fr->num;

    c->gop_size = 12;
    c->pix_fmt = pixfmt;
    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == CODEC_ID_MPEG1VIDEO){
        /* Needed to avoid using macroblocks in which some coeffs overflow.
           This does not happen with normal video, it just happens here as
           the motion of the chroma plane does not match the luma plane. */
        c->mb_decision=2;
    }
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}


static void close_audio(AVFormatContext *oc, AVStream *st)
{
    avcodec_close(st->codec);

    av_free(samples);
    av_free(audio_outbuf);
}



static void close_video(AVFormatContext *oc, AVStream *st)
{
    avcodec_close(st->codec);
    av_free(picture->data[0]);
    av_free(picture);
    if (tmp_picture) {
        av_free(tmp_picture->data[0]);
        av_free(tmp_picture);
    }
    av_free(video_outbuf);
}


int main(int argc, char **argv)
{
  
  // IO filename
  const char *i_filename;
  const char *o_filename;
  

  // Structures to write 
  AVOutputFormat  *out_fmt;
  AVFormatContext *out_fmt_ctx;
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
  
  out_fmt_ctx = avformat_alloc_context();
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

  if( out_fmt->video_codec != CODEC_ID_NONE) {
    out_stream_video = add_video_stream( out_fmt_ctx,
					 out_fmt->video_codec,
					 &framerate,
					 width,height,
					 PIX_FMT_YUV420P);
  }
  if ( out_fmt->audio_codec != CODEC_ID_NONE) {
    out_stream_audio = add_audio_stream( out_fmt_ctx, out_fmt->audio_codec);
  }
  av_set_parameters( out_fmt_ctx, NULL);

  dump_format( out_fmt_ctx, 0, o_filename, 1);

  if(out_stream_video) open_video( out_fmt_ctx, out_stream_video);
  if(out_stream_audio) open_audio( out_fmt_ctx, out_stream_audio);

  printf( "CODEC-ID:%i\n", out_fmt_ctx->oformat->video_codec);

  frame = avcodec_alloc_frame();

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
        struct SwsContext *pSWSCtx = sws_getContext( in_cdc_ctx->width,
						     in_cdc_ctx->height,
						     in_cdc_ctx->pix_fmt,
						     in_cdc_ctx->width,
						     in_cdc_ctx->height,
						     PIX_FMT_RGB24,
						     SWS_BICUBIC,
						     NULL,
						     NULL,
						     NULL);

	frame_rgb = avcodec_alloc_frame();
	if( frame_rgb == NULL) return;  
	num_bytes = avpicture_get_size( PIX_FMT_RGB24,
					in_cdc_ctx->width,
					in_cdc_ctx->height);
	buffer = (uint8_t*)av_malloc( num_bytes * sizeof(uint8_t));
	
	avpicture_fill( (AVPicture*)frame_rgb,
			buffer,
			PIX_FMT_RGB24,
			in_cdc_ctx->width,
			in_cdc_ctx->height);

        sws_scale( pSWSCtx,
		   frame->data,
		   frame->linesize,
		   0,
		   in_cdc_ctx->height,
		   frame_rgb->data,
		   frame_rgb->linesize);
        sws_freeContext(pSWSCtx);

	for( y = 0; y < in_cdc_ctx->height; y++){
	  for( x = 0; x < in_cdc_ctx->width; x++){
	    int p = x * 3 + y * frame_rgb->linesize[0];
	    frame_rgb->data[0][p]   = 0xff;
	    //pFrameRGB->data[0][p+1] = 0xff;
	    //pFrameRGB->data[0][p+2] = 0xff;
	  }
	}
	puts("test1");	
	if ( out_stream_audio){
	  audio_pts = (double)out_stream_audio->pts.val * out_stream_audio->time_base.num / out_stream_audio->time_base.den;
	} else{
	  audio_pts = 0.0;
	}
	
	if (out_stream_video){
	  video_pts = (double)out_stream_video->pts.val * out_stream_video->time_base.num / out_stream_video->time_base.den;
	}else{
	  video_pts = 0.0;
	}
	puts("test2");
	if (!out_stream_video
	    || ( out_stream_video && out_stream_audio  && audio_pts < video_pts)) {
	  puts("test3");
	  write_audio_frame( out_fmt_ctx, out_stream_audio);
	} else {
	  puts("test4");
	  write_video_frame( out_fmt_ctx, out_stream_video, frame_rgb);
	}

	av_free( buffer);
	i++;
      }
    }
    av_free_packet( &packet);
  }

  av_write_trailer( out_fmt_ctx);

  if (out_stream_video) close_video( out_fmt_ctx, out_stream_video);
  if (out_stream_audio) close_audio( out_fmt_ctx, out_stream_audio);

  for(i = 0; i < out_fmt_ctx->nb_streams; i++) {
    av_freep(&out_fmt_ctx->streams[i]->codec);
    av_freep(&out_fmt_ctx->streams[i]);
  }
  
  if (!(out_fmt->flags & AVFMT_NOFILE)) {
    url_fclose(out_fmt_ctx->pb);
  }


  av_free( frame);
  av_free( frame_rgb);
  avcodec_close( in_cdc_ctx);
  av_close_input_file( in_fmt_ctx);

  
  av_free( out_fmt_ctx);
  
  return 0;
  
}
