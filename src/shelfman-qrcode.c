/*
 * we only need one byte (gray) or one bit bw today.
 */

# include <stdlib.h>	// for free()
# include <stdio.h>		// for printf()
# include <string.h>	// for memset()
# include <errno.h>

#ifdef __linux__
# include <assert.h>	// for assert()
# include <fcntl.h>	// O_RDWR
# include <unistd.h>	// write()
# include <stdint.h>	// uint8_t, uint16_t
# include <time.h>
# include <sys/random.h>	// getrandom()
# define sleep_ms(n) usleep(1000*(n))
#else  // RP2040 Pico SDK
# include "rp2040.h"
# include "ptouch_rp2040.h"
# include "pico/stdlib.h"		// sleep_ms(), stdio_init_all()
#ifdef RAW_UART
# include "hardware/gpio.h"
# include "hardware/uart.h"		// needed for bypassing stdio only.
#endif

// # include "tusb.h"	// Includes tusb_config.h
#endif

#define DEBUG 1

#define BITS_PER_PIXEL 1	// 1 or 8.	both is implemented here.
#define BIG_FONT_SIZE 24
#define SMALL_FONT_SIZE 18
#define LINE_ADVANCE_FACTOR 1.9

struct qr_config {
	// config for brother D410
	unsigned max_height;		// my tape can print 120, although the printer could print 128.
	unsigned big_font_size;
	unsigned small_font_size;
	unsigned line_advance_perc;

	unsigned hspace;
	unsigned vspace;
	const char *title_text;
	const char *label_text_pre;
	const char *outfile;
};

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
  unsigned bits_per_val;
  unsigned char data[0];
};

struct img *img_new(unsigned w, unsigned h, int bits_per_val, unsigned char val)
{
	assert( (bits_per_val == 8) || (bits_per_val == 1) );

    int data_len = (bits_per_val == 1) ? (w * h / 8 + 1) : (w * h);
    struct img *im = (struct img *)calloc(sizeof(struct img) + data_len, 1);
    im->w = w; im->h = h;
	im->bits_per_val = bits_per_val;
	memset(im->data, val, data_len);
	return im;
}


void img_free(struct img *im)
{
    free((void *)(im));
}


unsigned get_pixel(struct img *im, int x, int y)
{
	// CAUTION: keep in sync with bits2img_fg() below.
    uint32_t pos = im->w * y + x;
	if (im->bits_per_val == 8)
		return im->data[pos];

	uint16_t byte_idx = (pos / 8);
	uint8_t bit_idx = 7 - (pos % 8);
	if (im->data[byte_idx] & (1 << bit_idx))
		return 255;
	return 0;
}


void set_pixel(struct img *im, int pos, int val)
{
	if (im->bits_per_val == 8)
		im->data[pos] = val;
	else
	{
		uint16_t byte_idx = (pos / 8);
		uint8_t bit_idx = 7 - (pos % 8);
		if (val)
			im->data[byte_idx] |= 1<<bit_idx;
		else
			im->data[byte_idx] &= ~(1<<bit_idx);
	}
}


void img_save(struct img *im, const char *filename)
{
#ifdef __linux__
    char buf[32];
    int len;
	if (im->bits_per_val == 8)
	    len = snprintf(buf, sizeof(buf), "P5\n%u %u\n255\n", im->w, im->h);
	else
	    len = snprintf(buf, sizeof(buf), "P1\n%u %u\n", im->w, im->h);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write(fd, buf, len);
	if (im->bits_per_val == 8)
		write(fd, im->data, im->w * im->h);	// pgm binary format is identical to what we store in struct img.
	else
	{
		// I am lazy: P4 pbm binary is slightly different than struct img: it needs line padding and has bits flipped.
		// P1 pbm ascii is simpler.
		unsigned pos = 0;
		for (unsigned y=0; y < im->h; y++)
		{
			for (unsigned x=0; x < im->w; x++)
			{
				write(fd, get_pixel(im, x, y) ? "0" : "1", 1);	// white = 0, black = 1, no whitespace needed.
				if (++pos % 64 == 0)
					write(fd, "\n", 1);
			}
		}
	}
    close(fd);
#else  // RP2040 Pico SDK
	// we don't have a filesystem, we debug print to stdout.
	printf("##########################\n");
	if (im->bits_per_val == 8)
	{
		printf("P2\n%u %u\n255\n", im->w, im->h);
		for (unsigned i=0; i < im->w * im->h; i++)
		{
			printf("%d ", im->data[i]);
			if (i % 32 == 31)
			   printf("\n");
		}
	}
	else
	{
		unsigned pos = 0;
        printf("P1\n%u %u\n", im->w, im->h);
		for (unsigned y=0; y < im->h; y++)
		{
			for (unsigned x=0; x < im->w; x++)
			{
				printf( get_pixel(im, x, y) ? "0" : "1" );	// white = 0, black = 1, no whitespace needed.
				if (++pos % 64 == 0)
					printf("\n");
			}
		}
	}
	printf("\n##########################\n");
#endif
}


void rectangle(struct img *im, unsigned x, unsigned y, unsigned w, unsigned h, unsigned val)
{
    // unsigned char *p = im->data + y*im->w + x;

    for (unsigned int j = 0; j < h; j++)
    {
		for (unsigned int i = 0; i < w; i++)
		{
		    if (((x + i) < (im->w)) &&
			    ((y + j) < (im->h)))
			{
				// p[j*im->w + i] = val;
				set_pixel(im, im->w * (y + j) + x + i, val);
			}
		}
	}
}


void blit(struct img *src, unsigned sx, unsigned sy, unsigned sw, unsigned sh,
          struct img *dst, unsigned dx, unsigned dy,
          unsigned char flags)
{
	assert(src->bits_per_val == dst->bits_per_val);
	// flags |= 0x40 : do not copy black pixels
	// flags |= 0x80 : do not copy white pixels
	// remaining bits: (flags & 0x3f):	spread, min 1.

	// unsigned char *ps = src->data + sy*src->w + sx;
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
				unsigned val = get_pixel(src, sx + i, sy + j);
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
	uint32_t r;
	for (int i = 0; i < 100; i++)
	{
		if (getrandom(&r, sizeof(r), 0)  == sizeof(r))
			return r;
		sleep_ms(100);
	}
#if DEBUG > 0
	printf("ERROR: getrandom failed 100 times. errno=%d\n", errno);
#endif
	return 0;
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

	enum qrcodegen_Ecc ecc = qrcodegen_Ecc_QUARTILE;

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


// set forground color in output pixels, where bitmap bit is set.
void bits2img_fg(const uint8_t *bitmap, struct img *output, uint16_t width, uint16_t height, unsigned fg)
{
    uint32_t pos = 0;

    // rectangle(output, 0, 0, width, height, bg);  // White background, should be done earler

    for(uint16_t y = 0; y < height; y++) {
        for(uint16_t x = 0; x < width; x++) {
            // Get bit: MSB first (bit 7 is leftmost pixel)
            uint16_t byte_idx = (pos / 8);
            uint8_t bit_idx = 7 - (pos % 8);
            if (bitmap[byte_idx] & (1 << bit_idx))
				set_pixel(output, pos, fg);
            pos++;
        }
    }
}


GFXglyph *extract_glyph(struct font *f, unsigned char ch, struct img *output, unsigned bits_per_val, unsigned fg)
{
    GFXglyph *glyph = &(f->ptr->glyph[ ch - f->ptr->first ]);

	if (ch < f->ptr->first || ch > f->ptr->last) return NULL;

    if (output)
		bits2img_fg(f->ptr->bitmap + glyph->bitmapOffset, output, glyph->width, glyph->height, fg);

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
			GFXglyph *g = extract_glyph(f, ch, NULL, 0, 0);
			if (!g)
				g = extract_glyph(f, '_', NULL, 0, 0);
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
		GFXglyph *g = extract_glyph(f, ch, NULL, 0, 0);
		if (!g)
		{
		    printf("ERROR: glyph not found: '%c' -> replacing with '_'\n", ch);
			ch = '_';
		    g = extract_glyph(f, ch, NULL, 0, 0);
		    if (!g)
			{
				printf("ERROR: replacment glyph also not found: '%c'\n", ch);
				exit(1);
			}
		}
		struct img *glyph_buf = img_new(g->width, g->height, BITS_PER_PIXEL, 255);

#if DEBUG > 1
		printf("glyph dimension of '%c' (%d x %d) @ xAdv=%d, xOff=%d, yOff=%d\n", text[c], g->width, g->height, g->xAdvance, g->xOffset, g->yOffset);
#endif
		(void)extract_glyph(f, ch, glyph_buf, BITS_PER_PIXEL, 0);
		blit(glyph_buf, 0, 0, g->width, g->height,
			 im, x + (f->scale * g->xOffset), y + (f->scale * (g->yOffset - f->max_asc)), f->scale);
		x += f->scale * g->xAdvance;
		img_free(glyph_buf);
	}
    return x - orig_x;
}


int gen_qrcode_tag(struct qr_config *cfg, const char *letter)
{
	unsigned width, height;
	char uid16[40];
	const char *code_text = "-";
	char label_text[40];
	snprintf(label_text, 40, "%s%s/", cfg->label_text_pre, letter);

    sprintf(uid16, "SFM-%s-", letter);
	hex16_string(uid16+strlen(uid16));
#if DEBUG > 0
	printf("uid16=%s\n", uid16);
#endif
	code_text = uid16;

    struct font *small_font = find_font(cfg->small_font_size);
    struct font *big_font   = find_font(cfg->big_font_size);

	// measure lengths
    unsigned title_w = draw_text(NULL, 0, 0, cfg->title_text, big_font, 0);
	unsigned label_w = draw_text(NULL, 0, 0, label_text, small_font, 0);
	unsigned code_w  = draw_text(NULL, 0, 0, code_text,  small_font, 0);

	unsigned max_text_w = 0;
	if (title_w > max_text_w) max_text_w = title_w;
	if (label_w > max_text_w) max_text_w = label_w;
	if (code_w  > max_text_w) max_text_w = code_w;

    unsigned computed_width = cfg->max_height + cfg->hspace + max_text_w + cfg->hspace;

#if DEBUG > 1
	printf("title_w=%d, label_w=%d, code_w=%d\n", title_w, label_w, code_w);
#endif
#if WITH_PNG_SUPPORT
    unsigned char *pngimage = NULL;
    if (cfg->input_png_file)
	{
		unsigned error = lodepng_decode32_file(&pngimage, &width, &height, cfg->input_png_file);
		if (error) {
			printf("%s: PNG error %u: %s\n", cfg->input_png_file, error, lodepng_error_text(error));
			return 1;
		}
		printf("Loaded PNG %ux%u\n", width, height);
#if DEBUG > 0
        if (width < c_width)
		    printf("WARNING: computed width for qr-code and text is %u\n", c_width);
#endif
	}
	else
#endif
	{
        width = computed_width;

		height = cfg->max_height;
#if DEBUG > 0
		printf("canvas size: %ux%u\n", width, height);
#endif
	}

    struct img *bw = img_new(width, height, BITS_PER_PIXEL, 255);

#if WITH_PNG_SUPPORT
    if (pngimage)
	{
		// pngimage is now width*height*4 RGBA bytes.
		// Convert to black and white, as bytes.
		for (unsigned int i = 0; i < width * height; i++) {
			if ((pngimage[4*i+3] < BW_THRESHOLD) || 	// ALPHA
			((pngimage[4*i+0] < BW_THRESHOLD) &&	// R
			 (pngimage[4*i+1] < BW_THRESHOLD) &&	// G
			 (pngimage[4*i+2] < BW_THRESHOLD)))	// B
				set_pixel(bw, i, 0);
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

    int y = cfg->vspace/2;
    // write some font,
#if 0
    draw_text(bw, qrsize, y,    "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.", small_font, 0);
    draw_text(bw, qrsize, y+50, "the quick brown fox jumps over the lazy dog.", big_font, 0);
#else
	draw_text(bw, qrsize + cfg->hspace + (int)((max_text_w - title_w)/2), y, cfg->title_text, big_font, 0);
	y = y + (int)(cfg->line_advance_perc * cfg->big_font_size / 100);
	draw_text(bw, qrsize + cfg->hspace + (int)((max_text_w - label_w)/2), y, label_text, small_font, 0);
	y = y + (int)(cfg->line_advance_perc * cfg->small_font_size / 100);
	draw_text(bw, qrsize + cfg->hspace + (int)((max_text_w - code_w)/2),  y, code_text,  small_font, 0);
#endif

#ifdef WITH_PNG_SUPPORT
    // with a loaded png, we have space to play around.
    blit(bw, 10, 150, 400, 20,  bw, 10, 180, 6|0x80); // zoom on the text
    blit(bw, 0, 0, (unsigned)qrsize, (unsigned)qrsize,  bw, 10, 300, 6|0x80); // zoom on QR code
#endif

#ifdef WITH_PNG_SUPPORT
    // FIXME, we should not save a PGM file here, we should save a proper PNG.
    img_save(bw, cfg->outfile);
#else
    // save as PGM
    img_save(bw, cfg->outfile);
#endif

    img_free(bw);
    return 0;
}


#ifndef __linux__ // RP2040 Pico SDK
#define LED_PIN 25

#define BLINK_DIT	blink(0)
#define BLINK_DAH	blink(1)

#if PICO_STDIO_USB_USE_TINYUSB
# define CONSOLE_READY stdio_usb_connected()
#elif PICO_STDIO_UART_ENABLED
# define CONSOLE_READY true
#else
# define CONSOLE_READY false	// neither usb nor uart configured.
#endif

struct qr_config *global_qrcode_cfg = NULL;

bool sleep100ms_bs(unsigned n)
{
    static bool prev_bootsel_state = 0;
	bool state = get_bootsel_button();

	tuh_task();

	for (unsigned i=0; i < n; i++)
	{
		sleep_ms(100);
		bool state = get_bootsel_button();	// from rp2040.c
		if (state != prev_bootsel_state)
		{
			prev_bootsel_state = state;
			if (state)
			{
				if (CONSOLE_READY)
					printf("BOOTSEL pressed!\n");
				gen_qrcode_tag(global_qrcode_cfg, "X");
			}
			else
			{
				if (CONSOLE_READY)
					printf("BOOTSEL released!\n");
			}
		}
	}
	return state;
}


int blink(bool dah)
{
	gpio_put(LED_PIN, 1);
	(void)sleep100ms_bs(dah?3:1);
	gpio_put(LED_PIN, 0);
	(void)sleep100ms_bs(1);
}
#endif // __linux__ // RP2040 Pico SDK


int main(int ac, char **av)
{
    struct qr_config cfg;
	// config for brother D410
	cfg.max_height = 120;		// my tape can print 120, although the printer could print 128.
	cfg.big_font_size = BIG_FONT_SIZE;
	cfg.small_font_size = SMALL_FONT_SIZE;
	cfg.line_advance_perc = (int)(100 * LINE_ADVANCE_FACTOR);
	cfg.hspace = 16;
	cfg.vspace = 8;
	cfg.title_text = "JW";
	cfg.label_text_pre = "shelfman.de/";

#if WITH_PNG_SUPPORT
	cfg.outfile = "output.pgm";	// FIXME: this should be a png file, see FIXME at end of main()
#else
	cfg.outfile = "output.pgm";	// FIXME: this should be pbm, if BITS_PER_PIXEL == 1
#endif

#if WITH_PNG_SUPPORT
	if (ac > 2)
		cfg.input_png_file = av[2];
	else
		cfg.input_png_file = NULL;
#endif

#ifdef __linux__

    srand(time(NULL));
    const char *letter = "X";
	if (ac > 1) letter = av[1];
    gen_qrcode_tag(&cfg, letter);

#else  // RP2040 Pico SDK

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    stdio_init_all();		// uart nr. and baud rate chosen in CMakeLists.txt via target_compile_definitions()

    // rtc_init();
	global_qrcode_cfg = &cfg;

	// say Hi in Morse code ...
    while (true)
	{
        BLINK_DIT; BLINK_DIT; BLINK_DIT; BLINK_DIT;
	    (void)sleep100ms_bs(2);	// total of 3. one already done in the last BLINK_DIT
        BLINK_DIT; BLINK_DIT;
	    (void)sleep100ms_bs(6);	// total of 7.
		// // if (CONSOLE_READY) 
		printf("Hi UART!\n");

        (void)sleep100ms_bs(10);
    }
#endif

	return 0;
}
