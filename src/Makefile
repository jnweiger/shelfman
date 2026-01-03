LODEPNG_DIR=../lodepng
GFXFONT_DIR=../Adafruit-GFX-Library
QRCODE_DIR=../QR-Code-generator/c
CFLAGS=-Wall -g -DLODEPNG_NO_COMPILE_ENCODER -DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS -DLODEPNG_NO_COMPILE_CPP
INC_DIRS=-I $(LODEPNG_DIR) -I $(GFXFONT_DIR) -I $(QRCODE_DIR)
DEPENDENCIES=$(QRCODE_DIR)/qrcodegen.c	$(LODEPNG_DIR)/lodepng.cpp

# need g++ here, so that lodepng.cpp is not ignored.

all: shelfman-qrcode.c
	g++ $(CFLAGS) $(INC_DIRS) -o shelfman-qrcode shelfman-qrcode.c $(DEPENDENCIES)

