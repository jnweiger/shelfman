/*
 * we only need one byte (gray) or one bit bw today.
 */
#include <stdlib.h>	// for free()
#include <stdio.h>	// for printf()
#include <fcntl.h>	// O_RDWR
#include <unistd.h>	// write()
#include <stdint.h>	// uint8_t, uint16_t

#include "lodepng.h"

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


void blit0(struct img *src, unsigned sx, unsigned sy, unsigned sw, unsigned sh,
           struct img *dst, unsigned dx, unsigned dy,
           unsigned char flags)
{
    // flags |= 0x40 : do not copy black pixels
    // flags |= 0x80 : do not copy white pixels
    // reaining bits: (flags & 0x3f):	spread, min 1.
    unsigned char *ps = src->data + sy*src->w + sx;
    unsigned char *pd = dst->data + dy*dst->w + dx;
    unsigned copy_b = (flags & 0x40) ? 0 : 1;
    unsigned copy_w = (flags & 0x80) ? 0 : 1;
    unsigned spread = (flags & 0x3f) | 0x01;

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
				    if (((dx + spread * i) < (dst->w)) &&
						((dy + spread * j) < (dst->h)))
				    {
					    // We are inside the dst image
					    pd[spread*j*dst->w + spread*i] = val;
				    }
				}
			}
		}
    }
}

void blit(struct img *src, unsigned sx, unsigned sy, unsigned sw, unsigned sh,
          struct img *dst, unsigned dx, unsigned dy,
          unsigned char flags)
{
    unsigned spread = (flags & 0x3f) | 0x01;
    for (unsigned j = 0; j < spread; j++)
    {
		for (unsigned i = 0; i < spread; i++)
		{
			blit0(src, sx, sy, sw, sh,  dst, dx+i, dy+j,  flags);
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


int main(int ac, char **av)
{
	unsigned char* image = NULL;
	unsigned width, height;

    unsigned error = lodepng_decode32_file(&image, &width, &height, av[1]);
    if (error) {
        printf("%s %s: PNG error %u: %s\n", av[0], av[1], error, lodepng_error_text(error));
        return 1;
    }
    printf("Loaded PNG %ux%u\n", width, height);

    struct img *bw = img_new(width, height, 255);

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

    // write some font,
    draw_text(bw, 10, 50, "Hello World", 20);
    blit(bw, 10, 50, 400, 20,  bw, 10, 80, 10|0x80);

    // save as PGM
    img_save(bw, "output.pgm");

    free(bw);
    return 0;
}

