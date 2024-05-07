#pragma warning(disable : 4996)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "video_decode.h"

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

#define IMG_WIDTH 1920
#define IMG_HEIGHT 1200
#define INBUF_SIZE IMG_WIDTH * IMG_HEIGHT

static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize, const char* filename)
{
	FILE* f;
	f = fopen(filename, "wb");
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	for (int i = 0; i < ysize; i++) {
		fwrite(buf + i * wrap, 1, xsize, f);
	}
	fclose(f);
}

void decode_internal(AVCodecContext* av_codec_ctx, AVFrame* av_frm, AVPacket* av_pkt, FILE* fp_out)
{
	static char buf[1024];
	int ret;
	// 将当前帧送入到解码器当中去解码
	ret = avcodec_send_packet(av_codec_ctx, av_pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding, error code:%d\n", ret);
		exit(1);
	}

	while (ret >= 0) {
		// 获取已经解码的数据
		ret = avcodec_receive_frame(av_codec_ctx, av_frm);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			return;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error during decoding, error code:%d\n", ret);
			exit(1);
		}

		fprintf(stderr, "saving frame %3" PRId64"\n", av_codec_ctx->frame_num);
		fflush(stdout);

		//snprintf(buf, sizeof(buf), "%s-%s" PRId64, out_filename, av_codec_ctx->frame_num);
		//pgm_save(av_frm->data[0], av_frm->linesize[0], av_frm->width, av_frm->height, out_filename);
		// 将已经解码的数据存储到文件中
		int size = av_frm->width * av_frm->height;
		fwrite(av_frm->data[0], 1, size, fp_out);//Y
		fwrite(av_frm->data[1], 1, size / 4, fp_out);//U
		fwrite(av_frm->data[2], 1, size / 4, fp_out);//V
	}
}


int decode(const char* in_file, const char* out_file)
{
	const AVCodec* av_codec;
	AVCodecParserContext* av_parser;
	AVCodecContext* av_codec_ctx = NULL;
	AVFrame* av_frame;
	uint8_t* data;
	size_t data_size;
	int ret;
	int eof;
	AVPacket* av_pkt;
	// malloc input buffer
	uint8_t* inbuf = (uint8_t*)malloc((INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE) * sizeof(uint8_t));
	if (!inbuf) {
		fprintf(stderr, "Error! alloc inbuf failed");
		exit(1);
	}
	memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	// create h264 decoder
	av_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!av_codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	// create ctx
	av_codec_ctx = avcodec_alloc_context3(av_codec);
	if (!av_codec_ctx) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	// creat pkt
	av_pkt = av_packet_alloc();
	if (!av_pkt) {
		fprintf(stderr, "Error! alloc pkt failed");
		exit(1);
	}

	// parse codec info
	av_parser = av_parser_init(av_codec->id);
	if (!av_parser) {
		fprintf(stderr, "parser not found\n");
		exit(1);
	}

	// create frame
	av_frame = av_frame_alloc();
	if (!av_frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	// open file
	FILE* fp_in = fopen(in_file, "rb");
	if (!fp_in) {
		fprintf(stderr, "Could not open %s\n", in_file);
		exit(1);
	}

	FILE* fp_out = fopen(out_file, "wb");
	if (!fp_out) {
		fprintf(stderr, "Could not open %s\n", out_file);
		exit(1);
	}

	// open dec codec
	if (avcodec_open2(av_codec_ctx, av_codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	do {
		/* read raw data from the input file */
		data_size = fread(inbuf, 1, INBUF_SIZE, fp_in);
		if (ferror(fp_in)) {
			break;
		}
		eof = !data_size;

		/* use the parser to split the data into frames */
		data = inbuf;
		while (data_size > 0 || eof) {
			// 从输入码流当中解析出一帧数据，送入到解码器当中解码
			// 如果是第1帧（IDR）的话，ret表示的索引还包括头信息（SPS+PPS+SEI）
			ret = av_parser_parse2(av_parser, av_codec_ctx, &av_pkt->data, &av_pkt->size,
				data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

			if (ret < 0) {
				fprintf(stderr, "Error! can not parse iput data");
				exit(1);
			}
			// 更新后面帧的起始地址
			data += ret;
			data_size -= ret;

			if (av_pkt->size) {
				// decode
				decode_internal(av_codec_ctx, av_frame, av_pkt, fp_out);
			}
			else if (eof) {
				break;
			}
		}
	} while (!eof);

	/* flush the decoder */
	decode_internal(av_codec_ctx, av_frame, NULL, fp_out);
	fclose(fp_in);
	free(inbuf);

	av_parser_close(av_parser);
	avcodec_free_context(&av_codec_ctx);
	av_frame_free(&av_frame);
	av_packet_free(&av_pkt);

	return 0;
}