#pragma warning(disable : 4996)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "streamer.h"

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
#include "libavutil/mathematics.h"
#include "libavutil/log.h"
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

int streamer_internal(const char* in_url, const char* out_url, bool is_sender)
{
	// set log level
	// av_log_set_level(AV_LOG_TRACE);

	AVOutputFormat* av_out_fmt = NULL;
	AVFormatContext* av_in_fmt_ctx = NULL;
	AVFormatContext* av_out_fmt_ctx = NULL;

	AVPacket av_pkt;
	const char* in_filename = in_url;
	const char* out_filename = out_url;
	int ret = 0;
	int i = 0;
	int video_idx = -1;
	int frame_idx = 0;
	int64_t start_time = 0;
	// bool b_sender = 0;

	//in_filename = "enc_in_all.flv"; // input flv file
	//out_filename = "rtmp://127.0.0.1:1935/live/stream"; // output url
	//in_filename = "rtmp://127.0.0.1:1935/live/stream"; // input flv file
	//out_filename = "receive.flv"; // output url

	// av_register_all(); // 新版本ffmpeg不再使用
	// init network
	avformat_network_init();

	if ((ret = avformat_open_input(&av_in_fmt_ctx, in_filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file.");
		goto end;
	}
	if ((ret = avformat_find_stream_info(av_in_fmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrive input stream information");
		goto end;
	}

	for (i = 0; i < av_in_fmt_ctx->nb_streams; i++) {
		if (av_in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_idx = i;
			break;
		}
	}
	// 将AVFormatContext结构体中媒体文件的信息进行格式化输出
	av_dump_format(av_in_fmt_ctx, 0, in_filename, 0);

	// Output
	// av_out_fmt_ctx是函数执行成功之后的上下文信息结构体
	// "flv"是输出格式
	// out_filename是输出文件
	ret = avformat_alloc_output_context2(&av_out_fmt_ctx, NULL, NULL, out_filename); // RTMP
	// ret = avformat_alloc_output_context2(&av_out_fmt_ctx, NULL, "flv", out_filename); // RTMP
	// avformat_alloc_output_context2(&av_out_fmt_ctx, NULL, "mpegts", out_filename); // UDP
	if (ret < 0) {
		fprintf(stderr, "Could not create output context, error code:%d\n", ret);
		//ret = AVERROR_UNKNOWN;
		goto end;
	}
	// av_out_fmt_ctx->oformat;
	for (i = 0; i < av_in_fmt_ctx->nb_streams; i++) {
		AVStream* in_stream = av_in_fmt_ctx->streams[i];
		// 为av_out_fmt_ctx创建一个新的流，第二个参数video_codec没有被使用
		AVStream* out_stream = avformat_new_stream(av_out_fmt_ctx, av_in_fmt_ctx->video_codec);
		//AVStream* out_stream = avformat_new_stream(av_out_fmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			fprintf(stderr, "Failed to allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		// Copy the setting of AVCodecContext
		// ret = avcodec_copy_context(out_stream->codecpar, in_stream->codecpar);
		ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
		if (ret < 0) {
			fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
			goto end;
		}

		out_stream->codecpar->codec_tag = 0;
		if (av_out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
			// out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			// out_stream->event_flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}
	}

	// Dump format
	// 将AVFormatContext结构体中媒体文件的信息进行格式化输出
	av_dump_format(av_out_fmt_ctx, 0, out_filename, 1);
	// Open output URL 
	if (!(av_out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		// 打开文件
		ret = avio_open(&av_out_fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output URL '%s'", out_filename);
			goto end;
		}
	}

	// Write file header
	ret = avformat_write_header(av_out_fmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occured when opening output URL\n");
		goto end;
	}

	if (is_sender) {
		start_time = av_gettime();
	}
	while (1) {
		AVStream* in_stream;
		AVStream* out_stream;

		// get an AVPacket
		// 这里如果使用av_pkt指针的话，第二帧时就会出错
		ret = av_read_frame(av_in_fmt_ctx, &av_pkt);
		if (ret < 0) {
			break;
		}

		// write pts
		if (av_pkt.pts == AV_NOPTS_VALUE && is_sender) {
			// write pts
			AVRational time_base1 = av_in_fmt_ctx->streams[video_idx]->time_base;
			// Duration between 2 frames (us)
			int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(av_in_fmt_ctx->streams[video_idx]->r_frame_rate);
			// parameters
			// pts是播放时间戳,告诉播放器什么时候播放这一帧视频,PTS通常是按照递增顺序排列的,以保证正确的时间顺序和播放同步
			// dts是解码时间戳,告诉播放器什么时候解码这一帧视频
			av_pkt.pts = (double)(frame_idx * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
			av_pkt.dts = av_pkt.pts;
			av_pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
		}

		// important: delay
		if (av_pkt.stream_index == video_idx && is_sender) {
			AVRational time_base = av_in_fmt_ctx->streams[video_idx]->time_base;
			AVRational time_base_q = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(av_pkt.dts, time_base, time_base_q);
			int64_t now_time = av_gettime() - start_time;
			if (pts_time > now_time) {
				av_usleep(pts_time - now_time);
			}
			// av_usleep(50);
		}

		in_stream = av_in_fmt_ctx->streams[av_pkt.stream_index];
		out_stream = av_out_fmt_ctx->streams[av_pkt.stream_index];

		// copy packet
		// convert PTS/DTS
		av_pkt.pts = av_rescale_q_rnd(av_pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		av_pkt.dts = av_rescale_q_rnd(av_pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		av_pkt.duration = av_rescale_q(av_pkt.duration, in_stream->time_base, out_stream->time_base);
		av_pkt.pos = -1;

		// print to screen
		if (av_pkt.stream_index == video_idx) {
			if (is_sender) {
				fprintf(stdout, "Send %8d video frames to output URL\n", frame_idx);
			}
			else {
				fprintf(stdout, "Receive %8d video frames from input URL\n", frame_idx);
			}

			frame_idx++;
		}

		ret = av_interleaved_write_frame(av_out_fmt_ctx, &av_pkt);
		// ret = av_write_frame(av_out_fmt_ctx, av_pkt);
		if (ret < 0) {
			fprintf(stderr, "Error muxing packet, error code:%d\n", ret);
			break;
		}
		// av_packet_free(&av_pkt);
		av_packet_unref(&av_pkt);
	}
	// write file trailer
	av_write_trailer(av_out_fmt_ctx);

end:
	avformat_close_input(&av_in_fmt_ctx);
	// close output
	/*if (av_out_fmt_ctx && !(av_out_fmt->flags & AVFMT_NOFILE)) {
		avio_close(av_out_fmt_ctx->pb);
	}*/

	/*avformat_free_context(av_out_fmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		fprintf(stderr, "Error occured\n");
		return -1;
	}*/
	return 0;
}

int streamer()
{
	const char* in_url = "rtmp://127.0.0.1:1935/live/stream"; // input flv file
	const char* out_url = "receive.flv"; // output url
	bool is_sender = 0;
	streamer_internal(in_url, out_url, is_sender);
	return 0;
}