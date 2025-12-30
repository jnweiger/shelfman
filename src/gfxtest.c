/*
 * we only need one byte (gray) or one bit bw today.
 */

#ifdef __linux__
# include <stdlib.h>	// for free()
# include <stdio.h>	// for printf()
# include <fcntl.h>	// O_RDWR
# include <unistd.h>	// write()
# include <stdint.h>	// uint8_t, uint16_t
# include <string.h>
# include <time.h>
#else  // RP2040 Pico SDK
# include "pico/stdlib.h"
# include "hardware/rtc.h"
# include "hardware/rng.h"
#endif

#include "lodepng.h"

#include "qrcodegen.h"

#define PROGMEM		/* NOOP */

#define _ADAFRUIT_GFX_H	/* so that nothing else gets included */
#include "gfxfont.h"
#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSans18pt7b.h"
#include "Fonts/FreeSans24pt7b.h"

// #include "Fonts/FreeSansBold12pt7b.h"
// #include "Fonts/FreeSansBold18pt7b.h"
// #include "Fonts/FreeSansBold24pt7b.h"
// #include "Fonts/FreeSansBold9pt7b.h"
// #include "Fonts/FreeSerif12pt7b.h"
// #include "Fonts/FreeSerif18pt7b.h"
// #include "Fonts/FreeSerif24pt7b.h"
// #include "Fonts/FreeSerif9pt7b.h"

struct font_table {
  unsigned size;
  unsigned scale;
  const GFXfont *ptr;
} fonts[] = {
  { 9,  1, &FreeSans9pt7b },
  { 12, 1, &FreeSans12pt7b },
  { 18, 1, &FreeSans18pt7b },
  { 24, 1, &FreeSans24pt7b },
  { 36, 2, &FreeSans18pt7b },
  { 48, 2, &FreeSans24pt7b },
  { 54, 3, &FreeSans18pt7b },
  { 72, 3, &FreeSans24pt7b }
};

#define BW_THRESHOLD 150	// something between 1 and 255, used for loading png.

struct img {
  unsigned w, h;
  unsigned char data[0];
};


struct img *img_new(unsigned w, unsigned h, unsigned char val)
{
    struct img *im = (struct img *)calloc(sizeof(struct img) + w * h, 1);
    im->w = w; im->h = h;
	memset(im->data, val, w * h);
	return im;
}


void img_save(struct img *im, const char *filename)
{
    char buf[32];
    int fd = open("output.pgm", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int len = snprintf(buf, sizeof(buf), "P5\n%u %u\n255\n", im->w, im->h);
    write(fd, buf, len);
    write(fd, im->data, im->w* im->h);
    close(fd);
}


void rectangle(struct img *im, unsigned x, unsigned y, unsigned w, unsigned h, unsigned val)
{
    unsigned char *p = im->data + y*im->w + x;

    for (unsigned int j = 0; j < h; j++)
    {
		for (unsigned int i = 0; i < w; i++)
		{
		    if (((x + i) < (im->w)) &&
			    ((y + j) < (im->h)))
			{
				p[j*im->w + i] = val;
			}
		}
	}
}


void blit(struct img *src, unsigned sx, unsigned sy, unsigned sw, unsigned sh,
          struct img *dst, unsigned dx, unsigned dy,
          unsigned char flags)
{
    // flags |= 0x40 : do not copy black pixels
    // flags |= 0x80 : do not copy white pixels
    // reaining bits: (flags & 0x3f):	spread, min 1.
    unsigned char *ps = src->data + sy*src->w + sx;
    // unsigned char *pd = dst->data + dy*dst->w + dx;
    unsigned copy_b = (flags & 0x40) ? 0 : 1;
    unsigned copy_w = (flags & 0x80) ? 0 : 1;
    unsigned spread = (flags & 0x3f);
	if (!spread) spread = 1;

    for (unsigned j = 0; j < sh; j++)
    {
		for (unsigned i = 0; i < sw; i++)
		{
		    if (((sx + i) < (src->w)) &&
			    ((sy + j) < (src->h)))
			{
			    // We are inside the src image
		        unsigned val = ps[j*src->w + i];
				if ((val == 0) ? copy_b : copy_w)
				{
			        rectangle(dst, dx + spread * i, dy + spread * j, spread, spread, val);
				}
			}
		}
    }
}


// returns width in pixels.
unsigned draw_text(struct img *im, unsigned x, unsigned y, const char *text, unsigned font_size)
{
    unsigned char *p = im->data + y*im->w + x;
    for (unsigned int i = 0; i < font_size; i++)
    {
        p[0] = 0;
        p[1] = 0;
        p[4] = 0;
        p += im->w;
        if (++y >= im->h)
          break;
    }
    return 4;
}

uint32_t rand32(void)
{
#ifdef __linux__
    return (rand() ^ (rand() << 15));	// make very sure, we have all 32bits.
#else
    uint32_t r;
    rng_hw_get_bytes(&r, sizeof(r));
    return r;
#endif
}

// needs buf[20]
void hex16_string(char *buf)
{
    uint32_t r = rand32();
    sprintf(buf, "%08x-%04x-%04x", rand32(), (r & 0xffff), (r >> 16));
}


int render_qrcode(struct img *im, unsigned x, unsigned y, unsigned margin, const char *ecc_letter, unsigned vers, const char *text, unsigned flags)
{
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
	uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

    unsigned copy_b = (flags & 0x40) ? 0 : 1;
    unsigned copy_w = (flags & 0x80) ? 0 : 1;
    unsigned spread = (flags & 0x3f);
	if (!spread) spread = 1;

	qrcodegen_Ecc ecc = qrcodegen_Ecc_QUARTILE;

	if      (ecc_letter[0] == 'L') ecc=qrcodegen_Ecc_LOW;
	else if (ecc_letter[0] == 'M') ecc=qrcodegen_Ecc_MEDIUM;
	else if (ecc_letter[0] == 'Q') ecc=qrcodegen_Ecc_QUARTILE;
	else if (ecc_letter[0] == 'H') ecc=qrcodegen_Ecc_HIGH;
	else printf("Unknown ecc letter '%s', expected L, M, Q, H\n", ecc_letter);

	bool ok = qrcodegen_encodeText(text, tempBuffer, qrcode, ecc, vers, vers, qrcodegen_Mask_AUTO, true);
	if (!ok) return -1;

	unsigned size = qrcodegen_getSize(qrcode);
    unsigned ss = size*spread+2*margin;

    if (copy_w && copy_b) rectangle(im, x, y, ss, ss, 255);	// paint background white

    for (unsigned int j = 0; j < size; j++)
    {
		for (unsigned int i = 0; i < size; i++)
		{
			unsigned val = qrcodegen_getModule(qrcode, i, j);
			if ((val > 0) ? copy_b : copy_w)
			{
			  rectangle(im, x+margin+spread*i, y+margin+spread*j, spread, spread, ((val > 0) ? 0 : 255));
			}
		}
	}
    return ss;
}


int main(int ac, char **av)
{
	unsigned char* image = NULL;
	unsigned width, height;
	char uid16[40];

#ifdef __linux__
    srand(time(NULL));
#else
    stdio_init_all();
    rtc_init();
#endif
    const char *letter = "X";
	if (ac > 1) letter = av[1];

    sprintf(uid16, "SFM-%s-", letter);
	hex16_string(uid16+strlen(uid16));
	printf("uid16=%s\n", uid16);

    if (ac > 2)
	{
		unsigned error = lodepng_decode32_file(&image, &width, &height, av[2]);
		if (error) {
			printf("%s %s %s: PNG error %u: %s\n", av[0], av[1], av[2], error, lodepng_error_text(error));
			return 1;
		}
		printf("Loaded PNG %ux%u\n", width, height);
	}
	else
	{
	    width = 300;
		height = 120;
		printf("canvas size: %ux%u\n", width, height);
	}

    struct img *bw = img_new(width, height, 255);

    if (ac > 2)
	{
		// image is now width*height*4 RGBA bytes.
		// Convert to black and white, as bytes.
		for (unsigned int i = 0; i < width * height; i++) {
			if ((image[4*i+3] < BW_THRESHOLD) || 	// ALPHA
			((image[4*i+0] < BW_THRESHOLD) &&	// R
			 (image[4*i+1] < BW_THRESHOLD) &&	// G
			 (image[4*i+2] < BW_THRESHOLD)))	// B
			bw->data[i] = 0;
		}
		free(image);
		// bw image is now width*height bytes 0 or 255.
	}

    unsigned qrsize = render_qrcode(bw, 150, 0, 2, "Q", 3, (const char *)uid16, 4);
	printf("qrcde size = %d\n", qrsize);

    // write some font,
    draw_text(bw, 10, 150, "Hello World", 20);
    blit(bw, 10, 150, 400, 20,  bw, 10, 180, 6|0x80); // zoom on the text
    blit(bw, 150, 0, qrsize, qrsize,  bw, 10, 300, 6|0x80); // zoom on QR code

    // save as PGM
    img_save(bw, "output.pgm");

    free(bw);
    return 0;
}

