/*
 * ptouch_rp2040.c – TinyUSB host backend for libptouch-like API
 *
 * USB connects to the printer. This can be done with a USB OTG cable with a micro-USB connector and female USB socket.
 * E.g. https://www.reichelt.de/de/de/shop/produkt/otg_kabel_usb_micro_b_stecker_auf_usb_2_0_a_buchse-129144
 *
 * References:
 * - https://sourcevu.sysprogs.com/rp2040/lib/tinyusb/symbols/tuh_descriptor_get_device
 *
 * There is no power supply coming from the printer. Thus we need to power the pico externally.
 * Pin 39 VSYS is for 3.8 V ... 5.5V input.
 * Pin 38: GND
 * Pin 6, (UART1 RX, GPIO5)	connects to Adapter TX
 * Pin 7, (UART1 RT, GPIO4)	connects to Adapter RX
 * Pin 8: GND (alternative GND, both are connected internally anyway.)
 *
 * CMakeLists.txt:
 * # Disable USB stdio (printer takes USB)
 * pico_enable_stdio_usb(qrcode 0)
 * # Enable UART stdio on UART1
 * pico_enable_stdio_uart(qrcode 1)
 *
 * Code:
 * stdio_init_all();  // printf() → UART1 (115200 baud)
 */


#include "ptouch_rp2040.h"	// Includes tusb_config.h via tusb.h
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define PTOUCH_VID   0x04F9   // Brother
#define PTOUCH_PID   0x20DF   // PT-D410 (from libptouch.c)

static uint8_t  g_dev_addr = 0;
static uint8_t  g_if_num   = 0;
static uint8_t  g_ep_out   = 0x00;   // bulk OUT endpoint address
static bool     g_ready    = false;


//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+

static void _convert_utf16le_to_utf8(const uint16_t* utf16, size_t utf16_len, uint8_t* utf8, size_t utf8_len) {
  // TODO: Check for runover.
  (void) utf8_len;
  // Get the UTF-16 length out of the data itself.

  for (size_t i = 0; i < utf16_len; i++) {
    uint16_t chr = utf16[i];
    if (chr < 0x80) {
      *utf8++ = chr & 0xffu;
    } else if (chr < 0x800) {
      *utf8++ = (uint8_t) (0xC0 | (chr >> 6 & 0x1F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (uint8_t) (0xE0 | (chr >> 12 & 0x0F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 6 & 0x3F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t* buf, size_t len) {
  size_t total_bytes = 0;
  for (size_t i = 0; i < len; i++) {
    uint16_t chr = buf[i];
    if (chr < 0x80) {
      total_bytes += 1;
    } else if (chr < 0x800) {
      total_bytes += 2;
    } else {
      total_bytes += 3;
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
  return (int) total_bytes;
}

static void print_utf16(uint16_t* temp_buf, size_t buf_len) {
  if ((temp_buf[0] & 0xff) == 0) return;  // empty
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t*) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t*) temp_buf)[utf8_len] = '\0';

  printf("%s", (char*) temp_buf);
}


// Declare for buffer for usb transfer, may need to be in USB/DMA section and
// multiple of dcache line size if dcache is enabled (for some ports).
CFG_TUH_MEM_SECTION struct {
  TUH_EPBUF_TYPE_DEF(tusb_desc_device_t, device);
  TUH_EPBUF_DEF(serial, 64*sizeof(uint16_t));
  TUH_EPBUF_DEF(buf, 128*sizeof(uint16_t));
} desc;


// TinyUSB host callbacks
void tuh_mount_cb(uint8_t dev_addr)
{
	tusb_desc_device_t *d = &desc.device;

    // Get Device Descriptor
    uint8_t xfer_result = tuh_descriptor_get_device_sync(dev_addr, d, sizeof(*d));
    if (XFER_RESULT_SUCCESS != xfer_result) {
		printf("tuh_mount_cb(%d): tuh_descriptor_get_device_sync() failed\n");
        return;
    }

#if DEBUG > 1
    printf("tuh_mount_cb(%d): found USB device: VID=%4x %s, PID=%4x %s\n", dev_addr,
		d->idVendor,  (d->idVendor  == PTOUCH_VID) ? "Brother" : "",
		d->idProduct, (d->idProduct == PTOUCH_PID) ? "PT-D410" : "");
#endif
    xfer_result = tuh_descriptor_get_serial_string_sync(dev_addr, LANGUAGE_ID, desc.serial, sizeof(desc.serial));
    if (XFER_RESULT_SUCCESS != xfer_result) {
        uint16_t* serial = (uint16_t*)(uintptr_t) desc.serial;
        serial[0] = 'n';
        serial[1] = '/';
        serial[2] = 'a';
        serial[3] = 0;
    }
    print_utf16((uint16_t*)(uintptr_t) desc.serial, sizeof(desc.serial)/2);
    printf("\r\n");

    printf("Device Descriptor:\r\n");
    printf("  bLength             %u\r\n", desc.device.bLength);
    printf("  bDescriptorType     %u\r\n", desc.device.bDescriptorType);
    printf("  bcdUSB              %04x\r\n", desc.device.bcdUSB);
    printf("  bDeviceClass        %u\r\n", desc.device.bDeviceClass);
    printf("  bDeviceSubClass     %u\r\n", desc.device.bDeviceSubClass);
    printf("  bDeviceProtocol     %u\r\n", desc.device.bDeviceProtocol);
    printf("  bMaxPacketSize0     %u\r\n", desc.device.bMaxPacketSize0);
    printf("  idVendor            0x%04x\r\n", desc.device.idVendor);
    printf("  idProduct           0x%04x\r\n", desc.device.idProduct);
    printf("  bcdDevice           %04x\r\n", desc.device.bcdDevice);

    // Get String descriptor using Sync API

    printf("  iManufacturer       %u     ", desc.device.iManufacturer);
    xfer_result = tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, desc.buf, sizeof(desc.buf));
    if (XFER_RESULT_SUCCESS == xfer_result) {
        print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
    }
    printf("\r\n");

    printf("  iProduct            %u     ", desc.device.iProduct);
    xfer_result = tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, desc.buf, sizeof(desc.buf));
    if (XFER_RESULT_SUCCESS == xfer_result) {
        print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
    }
    printf("\r\n");
    printf("  iSerialNumber       %u     ", desc.device.iSerialNumber);
    printf((char*)desc.serial); // serial is already to UTF-8
    printf("\r\n");

    printf("  bNumConfigurations  %u\r\n", desc.device.bNumConfigurations);

	// TODO: find interface number and configuraiton number for the bulk endpoint.

//     if (d->idVendor == PTOUCH_VID)
// 	{
//         uint8_t cfg[256];
//         uint16_t len = tuh_descriptor_get_configuration(dev_addr, 0, cfg, sizeof(cfg));
//         if (!len) return;
//
//         uint8_t const *p = cfg + sizeof(tusb_desc_configuration_t);
//         uint8_t const *end = cfg + len;
//
//         while (p < end) {
//             uint8_t const dlen  = p[0];
//             uint8_t const dtype = p[1];
//
//             if (dtype == TUSB_DESC_INTERFACE) {
//                 tusb_desc_interface_t const *itf = (tusb_desc_interface_t const *)p;
//                 g_if_num = itf->bInterfaceNumber;
// #if DEBUG > 1
// 				printf("tuh_mount_cb(%d): found bInterfaceNumber = %d\n", dev_addr, g_if_num);
// #endif
//             } else if (dtype == TUSB_DESC_ENDPOINT) {
//                 tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
//                 if ( (ep->bmAttributes & TUSB_XFER_BULK) &&
//                      !(ep->bEndpointAddress & TUSB_DIR_IN) ) {
//                     g_ep_out = ep->bEndpointAddress;  // bulk OUT
// #if DEBUG > 1
// 					printf("Found bulk OUT bEndpointAddress = %d\n", g_ep_out);
// #endif
//                 }
//             }
//
//             p += dlen;
//         }
//
//         g_dev_addr = dev_addr;
//         g_ready    = (g_ep_out != 0);
//     }
}

void tuh_umount_cb(uint8_t dev_addr)
{
    if (dev_addr == g_dev_addr) {
        g_dev_addr = 0;
        g_ready    = false;
        g_ep_out   = 0;
    }
#if DEBUG > 1
	printf("tuh_umount_cb(%d)\n", dev_addr);
#endif
}

// Synchronous-ish bulk OUT helper
static bool bulk_out_sync(const void *buf, uint32_t len, uint32_t timeout_ms)
{
    if (!g_ready) return false;

    volatile bool done = false;
    volatile bool ok   = false;

    usbh_xfer_t xfer =
    {
        .daddr   = g_dev_addr,
        .ep_addr = g_ep_out,
        .buf     = (void *)buf,
        .total_len = len,
        .complete_cb = [](usbh_xfer_t *x)
        {
            volatile bool *p_done = (volatile bool *)x->user_data;
            volatile bool *p_ok   = (volatile bool *)((uintptr_t)x->user_data + sizeof(bool));
            *p_ok   = (x->result == XFER_RESULT_SUCCESS);
            *p_done = true;
        },
        .user_data = NULL,
    };

    // Simple trick: pack pointers to flags into user_data
    struct {
        volatile bool *done;
        volatile bool *ok;
    } ctx = { &done, &ok };
    xfer.user_data = &ctx;

    if (!tuh_edpt_xfer(&xfer)) return false;

    uint32_t start = board_millis();
    while (!done) {
        tuh_task(); // must call frequently
        if ((board_millis() - start) > timeout_ms) {
            return false; // timeout
        }
    }
    return ok;
}

// Public “ptouch” API for reuse by the original code

int ptouch_open(void)
{
    // TinyUSB host is usually started in main() with tuh_init();
    // Here we just wait for the PT-D410 to appear.
    uint32_t start = board_millis();
    while (!g_ready) {
        tuh_task();
        if (board_millis() - start > 5000) {
            return -1; // no printer found within 5s
        }
    }
    return 0;
}

int ptouch_write(const void *buf, uint32_t len)
{
    // You can chunk to 64 bytes if desired, but TinyUSB will do it internally.
    if (!bulk_out_sync(buf, len, 2000)) {
        return -1;
    }
    return (int)len;
}

void ptouch_close(void)
{
    // Optional: you can force a reset or just leave it to USB unplug.
}

/*
int main(void)
{
    board_init();
    tusb_init(); // Host mode configured via tusb_config.h

    // tuh_mount_cb() → detect D410
    // tuh_printer_received_132cb() → status responses
    // tuh_printer_send() → PBM/PNG raster data


    // Wait for USB host to enumerate printer
    if (ptouch_open() != 0) {	// calls tuh_task() to process USB events (printer detection, data)
        // handle error
    }

    // Call the existing libptouch-style code, but link it against
    // ptouch_write()/ptouch_open()/ptouch_close() from above.
    // That code builds the Brother command stream from your PGM buffer.
}


*/
