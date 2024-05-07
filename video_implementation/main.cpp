#include "./include/video_decode.h"
#include "./include/video_encode.h"
#include "./include/muxer.h"
#include "./include/streamer.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	//printf("%d\n", argc);
	////bool encode = 0;
	//char* mode = NULL;
	//char* in_file = NULL;
	//char* out_file = NULL;
	//for (int i = 0; i < argc; ++i) {
	//	printf("i:%d, argv:%s\n", i, argv[i]);

	//	if (argv[i] == "--mode") {
	//		printf("%s\n", argv[i + 1]);
	//		mode = argv[i + 1];
	//	}
	//	/*else if (argv[i] == "--in_file") {
	//		in_file = argv[i + 1];
	//	}
	//	 else if (argv[i] == "--out_file") {
	//		out_file = argv[i + 1];
	//	}
	//	 else
	//	{
	//		break;
	//	}*/
	//}

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