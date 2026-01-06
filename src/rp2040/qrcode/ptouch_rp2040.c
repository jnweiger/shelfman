// ptouch_rp2040.c – TinyUSB host backend for libptouch-like API

#include "tusb.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define PTOUCH_VID   0x04F9   // Brother
#define PTOUCH_PID   0x2097   // example: replace with PT-D410 from libptouch.c

static uint8_t  g_dev_addr = 0;
static uint8_t  g_if_num   = 0;
static uint8_t  g_ep_out   = 0x00;   // bulk OUT endpoint address
static bool     g_ready    = false;

// TinyUSB host callbacks
void tuh_mount_cb(uint8_t dev_addr)
{
    tusb_desc_device_t const *desc = tuh_descriptor_device_get(dev_addr);
    if (!desc) return;

    if (desc->idVendor == PTOUCH_VID && desc->idProduct == PTOUCH_PID) {
        // Parse configuration descriptor to find bulk OUT endpoint
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
            } else if (dtype == TUSB_DESC_ENDPOINT) {
                tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
                if ( (ep->bmAttributes & TUSB_XFER_BULK) &&
                     !(ep->bEndpointAddress & TUSB_DIR_IN) ) {
                    g_ep_out = ep->bEndpointAddress;  // bulk OUT
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

    // Wait for USB host to enumerate printer
    if (ptouch_open() != 0) {
        // handle error
    }

    // Call the existing libptouch-style code, but link it against
    // ptouch_write()/ptouch_open()/ptouch_close() from above.
    // That code builds the Brother command stream from your PGM buffer.
}


*/
