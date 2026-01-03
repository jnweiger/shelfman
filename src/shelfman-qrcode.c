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

#define DEBUG 0
#define BIG_FONT_SIZE 24
#define SMALL_FONT_SIZE 18
#define LINE_ADVANCE_FACTOR 1.9

#ifndef WITH_PNG_SUPPORT
# define WITH_PNG_SUPPORT 1		// 1 or 0 to enable disable the png loader code.
#endif

#if WITH_PNG_SUPPORT
# include "lodepng.h"
#endif
// #define GLYPH_BUF_SIZE (24*48)		// 24 is the largest font size we have, and twice that wide is fairly large.

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

struct font {
  unsigned size;
  unsigned scale;
  int max_asc;			// initialized by find_font() - typically a negative number.
  const GFXfont *ptr;
} fonts[] = {
  { 9,  1, 0, &FreeSans9pt7b },
  { 12, 1, 0, &FreeSans12pt7b },
  { 18, 1, 0, &FreeSans18pt7b },
  { 24, 1, 0, &FreeSans24pt7b },
  { 36, 2, 0, &FreeSans18pt7b },
  { 48, 2, 0, &FreeSans24pt7b },
  { 54, 3, 0, &FreeSans18pt7b },
  { 72, 3, 0, &FreeSans24pt7b }
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

void img_free(struct img *im)
{
    free((void *)(im));
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


int find_highest_ascender(GFXglyph *g, int nglyphps)
{
    int off = g[0].yOffset;
	for (int i = 0; i < nglyphps; i++)
	{
		// larger y values downwards.
		if (off > g[i].yOffset)
			off = g[i].yOffset;
	}
	return off;
}


struct font *find_font(int size)
{
    for (int i = 0; i < (int)(sizeof(fonts)/sizeof(struct font)); i++)
    {
	    if (fonts[i].size >= (unsigned)size)
		{
			struct font *f = fonts+i;
			f->max_asc = find_highest_ascender(f->ptr->glyph, f->ptr->last - f->ptr->first);
#if DEBUG > 0
			printf("findfont(%d) -> size=%d, scale=%d, yAdvance=%d, max_asc=%d\n", size, f->size, f->scale, f->ptr->yAdvance, f->max_asc);
#endif
			return f;
		}
	}
	return NULL;
}


void bits2bytes(const uint8_t *bitmap, uint8_t *output, uint16_t width, uint16_t height, unsigned fg)
{
    uint16_t pos = 0;

    // memset(output, bg, width * height);  // White background, should be done earler

    for(uint16_t y = 0; y < height; y++) {
        for(uint16_t x = 0; x < width; x++) {
            // Get bit: MSB first (bit 7 is leftmost pixel)
            uint16_t byte_idx = (pos / 8);
            uint8_t bit_idx = 7 - (pos % 8);
            if (bitmap[byte_idx] & (1 << bit_idx))
				output[pos] = fg;
            pos++;
        }
    }
}


GFXglyph *extract_glyph(struct font *f, unsigned char ch, uint8_t *output, unsigned fg)
{
    GFXglyph *glyph = &(f->ptr->glyph[ ch - f->ptr->first ]);

	if (ch < f->ptr->first || ch > f->ptr->last) return NULL;

    if (output)
	    bits2bytes(f->ptr->bitmap + glyph->bitmapOffset, output, glyph->width, glyph->height, fg);

	return glyph;
}


// returns width in pixels.
unsigned draw_text(struct img *im, unsigned x, unsigned y, const char *text, struct font *f, unsigned val)
{
	unsigned orig_x = x;
	unsigned tlen = strlen(text);

	if (!im)
	{
		// measure length without drawing
		// CAUTION: keep in sync with drawing code below.
		for (unsigned c=0; c < tlen; c++)
		{
			char ch = text[c];
			GFXglyph *g = extract_glyph(f, ch, NULL, 0);
			if (!g)
				g = extract_glyph(f, '_', NULL, 0);
			if (g)
		        x += f->scale * g->xAdvance;
		}
		return x - orig_x;
	}

#if DEBUG > 0
    printf("%d,%d '%s' font size: %d, scale %d\n", x, y, text, f->size, f->scale);
#endif
	for (unsigned c=0; c < tlen; c++)
	{
		// CAUTION: keep in sync with measurement code above.
	    char ch = text[c];
		GFXglyph *g = extract_glyph(f, ch, NULL, 0);
		if (!g)
		{
		    printf("ERROR: glyph not found: '%c' -> replacing with '_'\n", ch);
			ch = '_';
		    g = extract_glyph(f, ch, NULL, 0);
		    if (!g)
			{
				printf("ERROR: replacment glyph also not found: '%c'\n", ch);
				exit(1);
			}
		}
		struct img *glyph_buf = img_new(g->width, g->height, 255);

#if DEBUG > 1
		printf("glyph dimension of '%c' (%d x %d) @ xAdv=%d, xOff=%d, yOff=%d\n", text[c], g->width, g->height, g->xAdvance, g->xOffset, g->yOffset);
#endif
		(void)extract_glyph(f, ch, glyph_buf->data, 0);
		blit(glyph_buf, 0, 0, g->width, g->height,
			 im, x + (f->scale * g->xOffset), y + (f->scale * (g->yOffset - f->max_asc)), f->scale);
		x += f->scale * g->xAdvance;
		img_free(glyph_buf);
	}
    return x - orig_x;
}


int main(int ac, char **av)
{
	// config for brother D410
	unsigned max_height = 120;		// my tape can print 120, although the printer could print 128.
	unsigned hspace = 16;
	unsigned vspace = 8;
	const char *title_text = "JW";
	const char *label_text_pre = "shelfman.de/";
	const char *code_text = "-";
#if WITH_PNG_SUPPORT
	const char *outfile = "shelfman_guid_qr.pgm";	// FIXME: this should be a png file, see FIXME at end of main()
#else
	const char *outfile = "shelfman_guid_qr.pgm";
#endif

#if WITH_PNG_SUPPORT
	unsigned char* pngimage = NULL;
#endif
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
	char label_text[40];
	snprintf(label_text, 40, "%s%s/", label_text_pre, letter);

    sprintf(uid16, "SFM-%s-", letter);
	hex16_string(uid16+strlen(uid16));
#if DEBUG > 0
	printf("uid16=%s\n", uid16);
#endif
	code_text = uid16;

    font *small_font = find_font(SMALL_FONT_SIZE);
    font *big_font   = find_font(BIG_FONT_SIZE);

	// measure lengths
    unsigned title_w = draw_text(NULL, 0, 0, title_text, big_font, 0);
	unsigned label_w = draw_text(NULL, 0, 0, label_text, small_font, 0);
	unsigned code_w  = draw_text(NULL, 0, 0, code_text,  small_font, 0);

	unsigned max_text_w = 0;
	if (title_w > max_text_w) max_text_w = title_w;
	if (label_w > max_text_w) max_text_w = label_w;
	if (code_w  > max_text_w) max_text_w = code_w;

#if DEBUG > 1
	printf("title_w=%d, label_w=%d, code_w=%d\n", title_w, label_w, code_w);
#endif
#if WITH_PNG_SUPPORT
    if (ac > 2)
	{
		unsigned error = lodepng_decode32_file(&pngimage, &width, &height, av[2]);
		if (error) {
			printf("%s %s %s: PNG error %u: %s\n", av[0], av[1], av[2], error, lodepng_error_text(error));
			return 1;
		}
		printf("Loaded PNG %ux%u\n", width, height);
	}
	else
#endif
	{
        width = max_height + hspace + max_text_w + hspace;

		height = max_height;
#if DEBUG > 0
		printf("canvas size: %ux%u\n", width, height);
#endif
	}

    struct img *bw = img_new(width, height, 255);

#if WITH_PNG_SUPPORT
    if (ac > 2)
	{
		// pngimage is now width*height*4 RGBA bytes.
		// Convert to black and white, as bytes.
		for (unsigned int i = 0; i < width * height; i++) {
			if ((pngimage[4*i+3] < BW_THRESHOLD) || 	// ALPHA
			((pngimage[4*i+0] < BW_THRESHOLD) &&	// R
			 (pngimage[4*i+1] < BW_THRESHOLD) &&	// G
			 (pngimage[4*i+2] < BW_THRESHOLD)))	// B
			bw->data[i] = 0;
		}
		free(pngimage);
		// bw image is now width*height bytes 0 or 255.
	}
#endif

    int qrsize = render_qrcode(bw, 0, 0, 2, "Q", 3, (const char *)uid16, 4);
#if DEBUG > 0
	printf("qrcde size = %d\n", qrsize);
#endif
	if (qrsize < 0) return 1;

    int y = vspace/2;
    // write some font,
#if 0
    draw_text(bw, qrsize, y,    "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.", small_font, 0);
    draw_text(bw, qrsize, y+50, "the quick brown fox jumps over the lazy dog.", big_font, 0);
#else
	draw_text(bw, qrsize + hspace + int((max_text_w - title_w)/2), y, title_text, big_font, 0);
	y = y + int(LINE_ADVANCE_FACTOR * BIG_FONT_SIZE);
	draw_text(bw, qrsize + hspace + int((max_text_w - label_w)/2), y, label_text, small_font, 0);
	y = y + int(LINE_ADVANCE_FACTOR * SMALL_FONT_SIZE);
	draw_text(bw, qrsize + hspace + int((max_text_w - code_w)/2),  y, code_text,  small_font, 0);
#endif

#ifdef WITH_PNG_SUPPORT
    // with a loaded png, we have space to play around.
    blit(bw, 10, 150, 400, 20,  bw, 10, 180, 6|0x80); // zoom on the text
    blit(bw, 0, 0, (unsigned)qrsize, (unsigned)qrsize,  bw, 10, 300, 6|0x80); // zoom on QR code
#endif

#ifdef WITH_PNG_SUPPORT
    // FIXME, we should not save a PGM file here, we should save a proper PNG.
    img_save(bw, outfile);
#else
    // save as PGM
    img_save(bw, outfile);
#endif

    img_free(bw);
    return 0;
}

