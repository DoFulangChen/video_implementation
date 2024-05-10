#include "./include/video_decode.h"
#include "./include/video_encode.h"
#include "./include/muxer.h"
#include "./include/streamer.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	// printf("%s\n", mode);
	/*const char* mode = "encode";*/
	const char* mode = "stream";
	const char* enc_in_file = "enc_in.yuv";
	const char* enc_out_file = "enc_out.bin";
	const char* dec_in_file = "dec_in.bin";
	const char* dec_out_file = "dec_out.yuv";
	if (mode == "encode") {
		encode(enc_in_file, enc_out_file);
	}
	else if (mode == "decode") {
		decode(dec_in_file, dec_out_file);
	}
	else if (mode == "mux") {
		muxer();
	}
	else if (mode == "stream") {
		streamer();
	}
	else {
		exit(1);
	}
	return 0;
}