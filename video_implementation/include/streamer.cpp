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

int streamer()
{
	AVOutputFormat* av_out_fmt = NULL;
	AVFormatContext* av_in_fmt_ctx = NULL;
	AVFormatContext* av_out_fmt_ctx = NULL;

	AVPacket* av_pkt;
	const char* in_filename;
	const char* out_filename;
	int ret = 0;
	int i = 0;
	int video_idx = -1;
	int frame_idx = 0;
	int64_t start_time = 0;

	in_filename = "out.flv"; // input flv file
	// out_filename = "rtmp://localhost/publishlive/livestream"; // output url // rtmp://localhost/live/livestream
	out_filename = "rtmp://localhost/live/livestream";

	// init network
	avformat_network_init();

	av_pkt = av_packet_alloc();

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

	av_dump_format(av_in_fmt_ctx, 0, in_filename, 0);

	// Output
	ret = avformat_alloc_output_context2(&av_out_fmt_ctx, NULL, "flv", out_filename); // RTMP
	// avformat_alloc_output_context2(&av_out_fmt_ctx, NULL, "mpegts", out_filename); // UDP

	if (ret < 0) {
		fprintf(stderr, "Could not create output context, error code:%d\n", ret);
		//ret = AVERROR_UNKNOWN;
		goto end;
	}

	// can not assign "const AVOutputFormat*" to "AVOutputFormat*"
	// av_out_fmt = av_out_fmt_ctx->oformat;
	for (i = 0; i < av_in_fmt_ctx->nb_streams; i++) {
		// Create ouput AVStream according to input AVStream
		AVStream* in_stream = av_in_fmt_ctx->streams[i];
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
			// out_stream->codecpar->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}
	}

	// Dump format
	av_dump_format(av_out_fmt_ctx, 0, out_filename, 1);
	// Open output URL
	if (!(av_out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
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

	start_time = (av_get_time_base_q().num / av_get_time_base_q().den);
	while (1) {
		AVStream* in_stream;
		AVStream* out_stream;

		// get an AVPacket
		ret = av_read_frame(av_in_fmt_ctx, av_pkt);
		if (ret < 0) {
			break;
		}

		// write pts
		if (av_pkt->pts == AV_NOPTS_VALUE) {
			// write pts
			AVRational time_base1 = av_in_fmt_ctx->streams[video_idx]->time_base;
			// Duration between 2 frames (us)
			int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(av_in_fmt_ctx->streams[video_idx]->r_frame_rate);
			// parameters
			av_pkt->pts = (double)(frame_idx * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
			av_pkt->dts = av_pkt->pts;
			av_pkt->duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
		}

		// important: delay
		if (av_pkt->stream_index == video_idx) {
			AVRational time_base = av_in_fmt_ctx->streams[video_idx]->time_base;
			AVRational time_base_q = { 1, AV_TIME_BASE };
			int64_t pts_time = av_rescale_q(av_pkt->dts, time_base, time_base_q);
			int64_t now_time = (av_get_time_base_q().num / av_get_time_base_q().den) - start_time;
			if (pts_time > now_time) {
				av_usleep(pts_time - now_time);

			}
		}

		in_stream = av_in_fmt_ctx->streams[av_pkt->stream_index];
		out_stream = av_out_fmt_ctx->streams[av_pkt->stream_index];

		// copy packet
		// convert PTS/DTS
		av_pkt->pts = av_rescale_q_rnd(av_pkt->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		av_pkt->dts = av_rescale_q_rnd(av_pkt->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		av_pkt->duration = av_rescale_q(av_pkt->duration, in_stream->time_base, out_stream->time_base);
		av_pkt->pos = -1;

		// print to screen
		if (av_pkt->stream_index == video_idx) {
			fprintf(stdout, "Send %8d video frames to output URL\n", frame_idx);
			frame_idx++;
		}

		ret = av_interleaved_write_frame(av_out_fmt_ctx, av_pkt);

		if (ret < 0) {
			fprintf(stderr, "Error muxing packet\n");
			break;
		}
		av_packet_free(&av_pkt);
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