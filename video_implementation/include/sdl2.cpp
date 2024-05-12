#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdl2.h"

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL.h"
#include "libavutil/imgutils.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL.h"
#include "libavutil/imgutils.h"
#ifdef __cplusplus
};
#endif
#endif

int sdl2()
{
	AVFormatContext* av_fmt_ctx;
	int i;
	int video_idx;
	AVCodecContext* av_codec_ctx;
	const AVCodec* av_codec;
	AVFrame* av_frm;
	AVFrame* av_frm_yuv;

	unsigned char* out_buffer;
	AVPacket* av_pkt;
	int y_size;
	int ret;
	int got_picture;
	struct SwsContext* img_convert_ctx;

	int screen_wdt = 0;
	int screen_hgt = 0;
	SDL_Window* sdl_window;
	SDL_Renderer* sdl_render;
	SDL_Texture* sdl_texture;
	SDL_Rect sdl_rect;

	FILE* fp_yuv;

	avformat_network_init();
	av_fmt_ctx = avformat_alloc_context();

	char file_path[] = "enc_out.bin";

	ret = avformat_open_input(&av_fmt_ctx, file_path, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "could not open input steram, error code:%d\n", ret);
		return -1;
	}

	ret = avformat_find_stream_info(av_fmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "could not find stream information, error code:%d\n", ret);
		return -1;
	}

	video_idx = -1;
	for (i = 0; i < av_fmt_ctx->nb_streams; i++) {
		if (av_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_idx = i;
			break;
		}
	}

	if (video_idx == -1) {
		fprintf(stderr, "could not find video stream\n");
		return -1;
	}

	// av_codec_ctx = av_fmt_ctx->streams[video_idx]->codecpar->codec_ctx;
	av_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!av_codec) {
		fprintf(stderr, "could not find encoder\n");
		return -1;
	}

	av_codec_ctx = avcodec_alloc_context3(av_codec);
	if (!av_codec_ctx) {
		fprintf(stderr, "could not alloc codec context\n");
		return -1;
	}

	ret = avcodec_open2(av_codec_ctx, av_codec, NULL);
	if (ret < 0) {
		fprintf(stderr, "could not open codec\n");
		return -1;
	}

	av_frm = av_frame_alloc();
	av_frm_yuv = av_frame_alloc();

	av_codec_ctx->width = av_fmt_ctx->streams[video_idx]->codecpar->width;
	av_codec_ctx->height = av_fmt_ctx->streams[video_idx]->codecpar->height;
	av_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

	out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, av_codec_ctx->width, av_codec_ctx->height, 1));
	// 根据长宽和颜色格式，将out_buffer的每个通道的首地址和每个通道的长度解析出来
	av_image_fill_arrays(av_frm_yuv->data, av_frm_yuv->linesize, out_buffer, AV_PIX_FMT_YUV420P, av_codec_ctx->width, av_codec_ctx->height, 1);
	av_pkt = (AVPacket*)av_malloc(sizeof(AVPacket));

	// output info
	fprintf(stdout, "--------------- File Information ----------------\n");
	av_dump_format(av_fmt_ctx, 0, file_path, 0);
	fprintf(stdout, "-------------------------------------------------\n");

	// 获取sws上下文结构体，是使用sws_scale()的基础
	img_convert_ctx = sws_getContext(av_codec_ctx->width, av_codec_ctx->height, av_codec_ctx->pix_fmt,
		av_codec_ctx->width, av_codec_ctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	// 初始化SDL
	// 视频模块 | 音频模块 | 计时器模块
	ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	if (ret < 0) {
		fprintf(stderr, "could not init sdl, error code:%d\n", ret);
		return -1;
	}
	screen_wdt = av_codec_ctx->width;
	screen_hgt = av_codec_ctx->height;

	// SDL2.0 support multiple windows
	// 创建SDL窗口，并指定OpenGL渲染
	// (name, rect.x, rect.y, rect.w, rect.h, render_library)
	sdl_window = SDL_CreateWindow("player window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_wdt, screen_hgt, SDL_WINDOW_OPENGL);
	if (!sdl_window) {
		fprintf(stderr, "SDL: could not create window - existing:%s\n", SDL_GetError());
		return -1;
	}
	// 创建渲染器，渲染的对象是sdl_window
	// (SDL_Window object, index, flags)
	// index = -1 表示使用第一个支持flags的渲染驱动进行渲染
	// 0表示不使用特定参数，如使用硬件渲染等
	sdl_render = SDL_CreateRenderer(sdl_window, -1, 0);
	// IYUV: Y + U + V (3 planes)
	// YV12: Y + V + U (3 planes)
	// 创建纹理信息，推送纹理到window的手是render
	// (render, format, access, wdt, hgt,)
	sdl_texture = SDL_CreateTexture(sdl_render, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, av_codec_ctx->width, av_codec_ctx->height);

	sdl_rect.x = 0;
	sdl_rect.y = 0;
	sdl_rect.w = screen_wdt;
	sdl_rect.h = screen_hgt;

	// frame process and render
	while (av_read_frame(av_fmt_ctx, av_pkt) >= 0) {
		if (av_pkt->stream_index == video_idx) {
			ret = avcodec_send_packet(av_codec_ctx, av_pkt);
			if (ret < 0) {
				fprintf(stderr, "decode error\n");
				return -1;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(av_codec_ctx, av_frm);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					return -1;
				}
				else if (ret < 0) {
					fprintf(stderr, "Error during decoding, error code:%d\n", ret);
					return -1;
				}

				// 裁剪图片，存储到av_frm_yuv当中
				sws_scale(img_convert_ctx, (const unsigned char* const*)av_frm->data, av_frm->linesize, 0, av_codec_ctx->height,
					av_frm_yuv->data, av_frm_yuv->linesize);
				// 根据av_frm_yuv的data，更新sdl_texture信息
				SDL_UpdateYUVTexture(sdl_texture, &sdl_rect,
					av_frm_yuv->data[0], av_frm_yuv->linesize[0],
					av_frm_yuv->data[1], av_frm_yuv->linesize[1],
					av_frm_yuv->data[2], av_frm_yuv->linesize[2]);

				SDL_RenderClear(sdl_render);								// 用绘图颜色清除当前渲染目标
				SDL_RenderCopy(sdl_render, sdl_texture, NULL, &sdl_rect);	// 将纹理的一部分复制到当前渲染目标
				SDL_RenderPresent(sdl_render);								// 使用自上次调用以来执行的任何呈现更新屏幕
				SDL_Delay(40); // delay 40ms
			}
		}
		av_packet_unref(av_pkt);
	}

	// flush decoder
	// ...

	sws_freeContext(img_convert_ctx);

	SDL_Quit();
	av_frame_free(&av_frm);
	av_frame_free(&av_frm_yuv);
	// avcodec_close(av_codec); // deprecate
	avformat_close_input(&av_fmt_ctx);
	return 0;
}