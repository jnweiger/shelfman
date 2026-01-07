#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "pico/rand.h"

int32_t getrandom(uint32_t *r, size_t n, int flags);

bool get_bootsel_button();

