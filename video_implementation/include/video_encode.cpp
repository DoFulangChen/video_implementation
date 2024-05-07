// video_implementation.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
// 解决调用fopen出现的warning
#pragma warning(disable : 4996)

#ifdef _WIN32
//Windows
extern "C" // 在C++文件中调用C文件需要使用，ffmpeg是使用C实现的
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#ifdef __cplusplus
};
#endif
#endif
#include "video_encode.h"

static void encode(AVCodecContext* avcodec_ctx, AVFrame* av_frm, AVPacket* av_pkt, FILE* out_file)
{
	int ret;

	/* Send frame to encoder */
	// ret 返回值的几种情况
	// EAGAIN 11 /* Try again */
	// ENOMEM 12 /* Out of memory */
	// EINVAL 22 /* Invalid argument */, 可能是编码器没有打开，或者是初始化时使用的是解码器
	// AVERROR_EOF /* End of File */
	// 0 /* Success */
	ret = avcodec_send_frame(avcodec_ctx, av_frm);
	if (ret < 0) {
		fprintf(stderr, "Error! fail to send frame, error code:%d", ret);
		exit(1);
	}

	while (ret >= 0) {
		/* Receive encoded frame */
		// EAGAIN 11 /* Try again */
		// EINVAL 22 /* Invalid argument */, 可能是编码器没有打开，或者是初始化时使用的是解码器
		// AVERROR_EOF /* End of File */
		ret = avcodec_receive_packet(avcodec_ctx, av_pkt);
		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
			return;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error! during encoding, error code:%d", ret);
			exit(1);
		}
		// unref将实例对应的内存释放掉
		// packet_unref针对于packet，每个函数都有一个对应的
		av_packet_unref(av_pkt);
	}
}

void encode(const char* in_file, const char* out_file)
{
	const AVCodec* avcodec;
	AVCodecContext* avcodec_ctx = NULL;
	AVPacket* av_pkt;
	AVFrame* av_frm;

	const char* file_name;
	const char* codec_name;
	uint8_t endcode[] = { 0, 0, 1, 0xb7 };

	int ret;

	FILE* f;
	//if (argc <= 2) {
	//	fprintf(stderr, "Usage: %s <output file> <codec name>\n", argv[0]);
	//	exit(0);
	//}

	//file_name = argv[1];
	/*codec_name = argv[2];*/

	/* create codec */
	// codec类型设置为264
	avcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!avcodec) {
		fprintf(stderr, "Error! can not find encoder");
		exit(1);
	}

	/* create context */
	// 创建编码器上下文信息
	avcodec_ctx = avcodec_alloc_context3(avcodec);
	if (!avcodec_ctx) {
		fprintf(stderr, "Error! can not alloc avcodec");
		exit(1);
	}

	/* create packet */
	// 创建packet数据类型
	av_pkt = av_packet_alloc();
	if (!av_pkt) {
		fprintf(stderr, "Error! alloc pkt failed");
		exit(1);
	}

	/* ctx init */
	avcodec_ctx->width = 1920;
	avcodec_ctx->height = 1080;
	avcodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

	/*
		time_base是一种时间戳计量基准，用于在多个结构体当中的time_base当中设置一个基准
		ffmpeg当中不同的结构体所使用的的time_base不统一，通过设置一个time_base，作为各个结构体之间数据转换的标准
	*/
	avcodec_ctx->time_base.den = 1;
	avcodec_ctx->time_base.num = 25;
	avcodec_ctx->framerate.den = 25;
	avcodec_ctx->framerate.num = 1;

	/* set context preset level */
	if (avcodec->id == AV_CODEC_ID_H264) {
		// 设置 preset = slow
		// 设置 zerolatency，在会议场景下是必然使用场景
		av_opt_set(avcodec_ctx->priv_data, "preset", "slow", 0);
		av_opt_set(avcodec_ctx->priv_data, "tune", "zerolatency", 0);
	}

	/* open codec */
	ret = avcodec_open2(avcodec_ctx, avcodec, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error! open avcodec failed, error code:%d", ret);
		exit(1);
	}

	/* file open */
	f = fopen(in_file, "wb");
	if (!f) {
		fprintf(stderr, "Error: open file failed");
		exit(1);
	}

	av_frm = av_frame_alloc();
	if (!av_frm) {
		fprintf(stderr, "Error: alloc frame failed");
		exit(1);
	}

	av_frm->width = avcodec_ctx->width;
	av_frm->height = avcodec_ctx->height;
	av_frm->format = avcodec_ctx->pix_fmt;

	/* get buffer */
	ret = av_frame_get_buffer(av_frm, 0);
	if (ret < 0) {
		fprintf(stderr, "Error: alloc frame data failed, error code:%d", ret);
		exit(1);
	}

	int x = 0;
	int y = 0;

	for (int i = 0; i < 25; i++) {
		fflush(stdout);
		/* Make sure the frame data is writable.
		   On the first round, the frame is fresh from av_frame_get_buffer()
		   and therefore we know it is writable.
		   But on the next rounds, encode() will have called
		   avcodec_send_frame(), and the codec may have kept a reference to
		   the frame in its internal structures, that makes the frame
		   unwritable.
		   av_frame_make_writable() checks that and allocates a new buffer
		   for the frame only if necessary.
		 */
		ret = av_frame_make_writable(av_frm);
		if (ret < 0) {
			fprintf(stderr, "Error: can not make frame writable");
			exit(1);
		}

		/* Prepare dummy image */
		/* Componet Y */
		for (y = 0; y < avcodec_ctx->height; y++) {
			for (x = 0; x < avcodec_ctx->width; x++) {
				av_frm->data[0][y * av_frm->linesize[0] + x] = 240;
			}
		}

		/* Component Cb and Cr */
		for (y = 0; y < avcodec_ctx->height / 2; y++) {
			for (x = 0; x < avcodec_ctx->width / 2; x++) {
				av_frm->data[1][y * av_frm->linesize[1] + x] = 128;
				av_frm->data[2][y * av_frm->linesize[2] + x] = 64;

			}
		}

		av_frm->pts = i;

		/* encode the img */
		encode(avcodec_ctx, av_frm, av_pkt, f);
	}

	/* flush the encoder */
	encode(avcodec_ctx, NULL, av_pkt, f);

	//if (avcodec->id == AV_CODEC_ID_MPEG1VIDEO || avcodec->id == AV_CODEC_ID_MPEG2VIDEO)
	//	fwrite(endcode, 1, sizeof(endcode), f);
	//fclose(f);

	// 释放内存
	avcodec_free_context(&avcodec_ctx);
	av_frame_free(&av_frm);
	av_packet_free(&av_pkt);
}
