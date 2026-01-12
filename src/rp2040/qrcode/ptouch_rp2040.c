/*
 * ptouch_rp2040.c – TinyUSB host backend for libptouch-like API
 *
 * USB connects to the printer. This can be done with a USB OTG cable with a micro-USB connector and female USB socket.
 * E.g. https://www.reichelt.de/de/de/shop/produkt/otg_kabel_usb_micro_b_stecker_auf_usb_2_0_a_buchse-129144
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


#include "tusb.h"	// Includes tusb_config.h
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define PTOUCH_VID   0x04F9   // Brother
#define PTOUCH_PID   0x20DF   // PT-D410 (from libptouch.c)

static uint8_t  g_dev_addr = 0;
static uint8_t  g_if_num   = 0;
static uint8_t  g_ep_out   = 0x00;   // bulk OUT endpoint address
static bool     g_ready    = false;

// TinyUSB host callbacks
void tuh_mount_cb(uint8_t dev_addr)
{
    tusb_desc_device_t const *desc = tuh_descriptor_device_get(dev_addr);
    if (!desc) return;

#if DEBUG > 1
    printf("tuh_mount_cb(%d): found USB device: VID=%4x %s, PID=%4x %s\n", dev_addr,
		desc->idVendor,  (desc->idVendor  == PTOUCH_VID) ? "Brother" : "",
		desc->idProduct, (desc->idProduct == PTOUCH_PID) ? "PT-D410" : "");
#endif
    if (desc->idVendor == PTOUCH_VID)
        // It is one of the brother printers. Parse configuration descriptor to find bulk OUT endpoint
        uint8_t cfg[256];
        uint16_t len = tuh_descriptor_get_configuration(dev_addr, 0, cfg, sizeof(cfg));
        if (!len) return;

        uint8_t const *p = cfg + sizeof(tusb_desc_configuration_t);
        uint8_t const *end = cfg + len;

        while (p < end) {
            uint8_t const dlen  = p[0];
            uint8_t const dtype = p[1];

            if (dtype == TUSB_DESC_INTERFACE) {
                tusb_desc_interface_t const *itf = (tusb_desc_interface_t const *)p;
                g_if_num = itf->bInterfaceNumber;
#if DEBUG > 1
				printf("tuh_mount_cb(%d): found bInterfaceNumber = %d\n", dev_addr, g_if_num);
#endif
            } else if (dtype == TUSB_DESC_ENDPOINT) {
                tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
                if ( (ep->bmAttributes & TUSB_XFER_BULK) &&
                     !(ep->bEndpointAddress & TUSB_DIR_IN) ) {
                    g_ep_out = ep->bEndpointAddress;  // bulk OUT
#if DEBUG > 1
					printf("Found bulk OUT bEndpointAddress = %d\n", g_ep_out);
#endif
                }
            }

            p += dlen;
        }

        g_dev_addr = dev_addr;
        g_ready    = (g_ep_out != 0);
    }
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

    // tuh_printer_mount_cb() → detect D410
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
