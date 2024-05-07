// video_implementation.cpp : ���ļ����� "main" ����������ִ�н��ڴ˴���ʼ��������
//
// �������fopen���ֵ�warning
#pragma warning(disable : 4996)

#ifdef _WIN32
//Windows
extern "C" // ��C++�ļ��е���C�ļ���Ҫʹ�ã�ffmpeg��ʹ��Cʵ�ֵ�
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
	// ret ����ֵ�ļ������
	// EAGAIN 11 /* Try again */
	// ENOMEM 12 /* Out of memory */
	// EINVAL 22 /* Invalid argument */, �����Ǳ�����û�д򿪣������ǳ�ʼ��ʱʹ�õ��ǽ�����
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
		// EINVAL 22 /* Invalid argument */, �����Ǳ�����û�д򿪣������ǳ�ʼ��ʱʹ�õ��ǽ�����
		// AVERROR_EOF /* End of File */
		ret = avcodec_receive_packet(avcodec_ctx, av_pkt);
		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
			return;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error! during encoding, error code:%d", ret);
			exit(1);
		}
		// unref��ʵ����Ӧ���ڴ��ͷŵ�
		// packet_unref�����packet��ÿ����������һ����Ӧ��
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
	// codec��������Ϊ264
	avcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!avcodec) {
		fprintf(stderr, "Error! can not find encoder");
		exit(1);
	}

	/* create context */
	// ������������������Ϣ
	avcodec_ctx = avcodec_alloc_context3(avcodec);
	if (!avcodec_ctx) {
		fprintf(stderr, "Error! can not alloc avcodec");
		exit(1);
	}

	/* create packet */
	// ����packet��������
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
		time_base��һ��ʱ���������׼�������ڶ���ṹ�嵱�е�time_base��������һ����׼
		ffmpeg���в�ͬ�Ľṹ����ʹ�õĵ�time_base��ͳһ��ͨ������һ��time_base����Ϊ�����ṹ��֮������ת���ı�׼
	*/
	avcodec_ctx->time_base.den = 1;
	avcodec_ctx->time_base.num = 25;
	avcodec_ctx->framerate.den = 25;
	avcodec_ctx->framerate.num = 1;

	/* set context preset level */
	if (avcodec->id == AV_CODEC_ID_H264) {
		// ���� preset = slow
		// ���� zerolatency���ڻ��鳡�����Ǳ�Ȼʹ�ó���
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

	// �ͷ��ڴ�
	avcodec_free_context(&avcodec_ctx);
	av_frame_free(&av_frm);
	av_packet_free(&av_pkt);
}
