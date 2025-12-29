LODEPNG_DIR=../lodepng
CFLAGS=-Wall -g -DLODEPNG_NO_COMPILE_ENCODER -DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS -DLODEPNG_NO_COMPILE_CPP 
# need g++ here, so that lodepng.cpp is not ignored.

all: gfxtest.c
	g++ $(CFLAGS) -I $(LODEPNG_DIR) -o gfxtest gfxtest.c $(LODEPNG_DIR)/lodepng.cpp

