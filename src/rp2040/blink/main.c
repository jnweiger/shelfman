
#include <stdio.h>
#include <stdbool.h>

#ifdef TARGET_PICO
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#endif

#ifdef TARGET_LINUX
# include <unistd.h>	// usleep()
# define sleep_ms(ms) usleep((ms)*1000)
# define gpio_init(pin)
# define gpio_set_dir_out(pin)
# define gpio_set_dir(pin, dir)
# define gpio_put(pin, val)
# define stdio_init_all()
# define  stdio_usb_connected() 0
:-(
#endif

#ifdef PICO_TARGET
// From pico-examples/common/get_bootsel_button.c
bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;

    // Disable interrupts (flash access might be interrupted)
    uint32_t flags = save_and_disable_interrupts();

    // Float the flash CS pin (Hi-Z)
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Small delay (can't sleep, no flash access)
    for (volatile int i = 0; i < 1000; ++i);

    // Read pin state via SIO (button pulls low when pressed)
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Restore flash CS
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);
    return button_state;
}
#else
bool get_bootsel_button() { return 0; }
#endif


#define LED_PIN 25

#define BLINK_DIT	blink(0)
#define BLINK_DAH	blink(1)

bool sleep100ms_bs(unsigned n)
{
    static bool prev_bootsel_state = 0;
	bool state = get_bootsel_button();

	for (unsigned i=0; i < n; i++)
	{
		sleep_ms(100);
		bool state = get_bootsel_button();
		if (state != prev_bootsel_state)
		{
			prev_bootsel_state = state;
			if (state)
			{
				if (stdio_usb_connected())
					printf("BOOTSEL pressed!\n");
			}
			else
			{
				if (stdio_usb_connected())
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

int main() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
	stdio_init_all();

	// say Hi
    while (true)
	{
        BLINK_DIT; BLINK_DIT; BLINK_DIT; BLINK_DIT;
	    (void)sleep100ms_bs(2);	// total of 3. one already done in the last BLINK_DIT
        BLINK_DIT; BLINK_DIT;
	    (void)sleep100ms_bs(6);	// total of 7.
    }
}

