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

all: linux rp2040

.PHONY: linux
linux: shelfman-qrcode

shelfman-qrcode: shelfman-qrcode.c
	g++ $(CFLAGS) $(INC_DIRS) -o shelfman-qrcode shelfman-qrcode.c $(DEPENDENCIES)

.PHONY: rp2040 clean upload
rp2040:
	mkdir -p rp2040/blink/build
	cd rp2040/blink/build; cmake .. && make
	mkdir -p rp2040/qrcode/build
	cd rp2040/qrcode/build; cmake .. && make
	mkdir -p rp2040/uart_test/build
	cd rp2040/uart_test/build; cmake .. && make

# UPLOAD_NAME=uart_test
UPLOAD_NAME=qrcode

clean:
	rm -f *.o shelfman-qrcode
	cd rp2040/blink/build; test -f Makefile && make clean || true
	cd rp2040/qrcode/build; test -f Makefile && make clean || true
	cd rp2040/uart_test/build; test -f Makefile && make clean || true

upload install:
	@sd=$$(dirname /media/$$USER/*/INDEX.HTM); test "$$sd" != "/media/$$USER/*" && (set -x; cp rp2040/$(UPLOAD_NAME)/build/$(UPLOAD_NAME).uf2 $$sd ) || echo "ERROR: rp2040 usb drive not found. Disconnect USB, press&hold BOOTSEL button, connect USB, wait 3 seconds, release BOOTSEL, then try again."


