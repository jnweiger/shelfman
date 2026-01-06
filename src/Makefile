LODEPNG_DIR=../lodepng
GFXFONT_DIR=../Adafruit-GFX-Library
QRCODE_DIR=../QR-Code-generator/c
PTOUCH_DIR=../ptouch-print/src
# CFLAGS=-Wall -g -DLODEPNG_NO_COMPILE_ENCODER -DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS -DLODEPNG_NO_COMPILE_CPP
CFLAGS=-Wall -g -DWITH_PNG_SUPPORT=0
INC_DIRS=-I $(LODEPNG_DIR) -I $(GFXFONT_DIR) -I $(QRCODE_DIR)
# DEPENDENCIES=$(QRCODE_DIR)/qrcodegen.c	$(LODEPNG_DIR)/lodepng.cpp
DEPENDENCIES=$(QRCODE_DIR)/qrcodegen.c

# need g++ here, so that lodepng.cpp is not ignored.

all: shelfman-qrcode rp2040
	g++ $(CFLAGS) $(INC_DIRS) -o shelfman-qrcode shelfman-qrcode.c $(DEPENDENCIES)

.PHONY: rp2040
rp2040:
	mkdir -p rp2040/blink/build
	cd rp2040/blink/build; cmake .. && make
	mkdir -p rp2040/build
	cd rp2040/build; cmake .. && make
