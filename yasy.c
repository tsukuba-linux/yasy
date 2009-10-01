#include <stdio.h>
#include <string.h>
#include <cv.h>
#include <cxcore.h>
#include <highgui.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "option.h"

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


int load_frames( const char* filename, AVFrame** frames){
  int i;
  int video_stream;
  int audio_stream;
  int num_bytes;
  int frame_finished;
  uint8_t* buffer;

  AVFormatContext *pFormatCtx;
  AVCodecContext *pCodecCtx;
  AVCodec *pCodec;
  AVFrame *pFrame;
  AVFrame *pFrameRGB;
  AVPacket packet;

  // Open video file
  if( av_open_input_file( &pFormatCtx, filename, NULL, 0, NULL) != 0){
    return;
  }
  dump_format( pFormatCtx, 0, filename, 0);

  // Find video stream and get codec information
  video_stream = -1;
  for( i = 0; i < pFormatCtx->nb_streams; i++){
    switch(pFormatCtx->streams[i]->codec->codec_type){
    case CODEC_TYPE_VIDEO:
      video_stream = i;
      break;      
    case CODEC_TYPE_AUDIO:
      audio_stream = i;
      break;      
    }
  }
  if( video_stream == -1) return; // no video streams
  pCodecCtx = pFormatCtx->streams[video_stream]->codec;

  // Find and open codec (if it is found
  pCodec = avcodec_find_decoder( pCodecCtx->codec_id);
  if( pCodec == NULL){
    // No decoder
    fprintf(stderr, "Unsupported codec!\n");
    return;
  }
  if( avcodec_open(pCodecCtx, pCodec) < 0 ) return;

  pFrame    = avcodec_alloc_frame();

  i = 0;
  while( av_read_frame( pFormatCtx, &packet) >= 0){
    int x;
    int y;
    // Check if this packet is from video stream
    if( packet.stream_index == video_stream){
      avcodec_decode_video( pCodecCtx,
			    pFrame,
			    &frame_finished,
			    packet.data,
			    packet.size);
      if( frame_finished){
	printf( "%i, %i\n", pCodecCtx->width, pCodecCtx->height);

        struct SwsContext *pSWSCtx = sws_getContext( pCodecCtx->width,
						     pCodecCtx->height,
						     pCodecCtx->pix_fmt,
						     pCodecCtx->width,
						     pCodecCtx->height,
						     PIX_FMT_RGB24,
						     SWS_BICUBIC,
						     NULL,
						     NULL,
						     NULL);

	pFrameRGB = avcodec_alloc_frame();
	if( pFrameRGB == NULL) return;  
	num_bytes = avpicture_get_size( PIX_FMT_RGB24,
					pCodecCtx->width,
					pCodecCtx->height);
	buffer = (uint8_t*)av_malloc( num_bytes * sizeof(uint8_t));
	
	avpicture_fill( (AVPicture*)pFrameRGB,
			buffer,
			PIX_FMT_RGB24,
			pCodecCtx->width,
			pCodecCtx->height);

        sws_scale( pSWSCtx,
		   pFrame->data,
		   pFrame->linesize,
		   0,
		   pCodecCtx->height,
		   pFrameRGB->data,
		   pFrameRGB->linesize);
        sws_freeContext(pSWSCtx);

	for( y = 0; y < pCodecCtx->height; y++){
	  for( x = 0; x < pCodecCtx->width; x++){
	    int p = x * 3 + y * pFrameRGB->linesize[0];
	    pFrameRGB->data[0][p]   = 0xff;
	    //pFrameRGB->data[0][p+1] = 0xff;
	    //pFrameRGB->data[0][p+2] = 0xff;
	  }
	}
	//SaveFrame( pFrameRGB, 320, 240, i);
	if( frames != NULL){
	  *(frames + i) = pFrameRGB;
	}
	i++;
      }
    }
    av_free_packet( &packet);
  }

  av_free( pFrame);
  av_free( buffer);
  avcodec_close( pCodecCtx);
  av_close_input_file(pFormatCtx);

  return i;

}

int main( int argc, char** argv) {
  
  int nb_frames;
  int i;
  char* input_filename;
  AVFrame** frames;

  parse_options( &argc, &argv);

  if( argc < 1){
    return -1;
  }
  input_filename = argv[0];


  // Initialize
  av_register_all();

  nb_frames = load_frames( input_filename, NULL);
  frames = (AVFrame**)malloc( nb_frames * sizeof(AVFrame*));
  load_frames( input_filename, frames);

  for( i = 0; i < nb_frames; i++){
    SaveFrame( frames[i], 320, 240, i);
  }

  free( frames);
  printf("%i\n", nb_frames);
  puts("ALL YOUR BASES ARE BELONG TO US");

  return 0;
}
