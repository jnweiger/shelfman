#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

// References:
// - https://github.com/hathach/tinyusb

// Enable Host mode (critical!)
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_HOST)




// No HID or other classes needed
#define CFG_TUH_HID             0

// tusb_config.h for HOST mode
#define CFG_TUH_ENABLED         1
#define CFG_TUH_DEVICE_MAX      1  // expect 1 printer
#define CFG_TUH_INTERFACE_MAX   4


// Enable TinyUSB classes
#define CFG_TUSB_HOST_DEVICE_MAX    1  // Single printer
#define CFG_TUH_CDC                 0
#define CFG_TUH_HID                 0
#define CFG_TUH_MSC                 0

// Generic vendor class for bulk transfers (printer is not standard class)
#define CFG_TUH_VENDOR              0	// this is a broken in tinyusb(), not fully migrated.

// Buffer sizes
#define CFG_TUH_HUB                 0
#define CFG_TUSB_HOST_HID_MAX_REPORT 64

// Memory optimization
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))

#endif

