#include "hardware.h"

#define AMP_CONTROL 29

void hardware_init()
{
    gpio_init(AMP_CONTROL);
    gpio_set_dir(AMP_CONTROL, GPIO_OUT);
}

static bool last_amp_state;

void hardware_set_amp(bool amp_state)
{
    hardware_set_amp_force(amp_state, false);
}

void hardware_set_amp_force(bool amp_state, bool force)
{
    if(force || last_amp_state != amp_state)
    {
        gpio_put(AMP_CONTROL, amp_state);
        last_amp_state = amp_state;
    }
}
static bool last_mic_state = false;

void hardware_set_mic(bool mic_state)
{
    // if we want to turn on, and the amp is off (i.e. no feedback)
    if(!last_amp_state && mic_state && last_mic_state != mic_state)
    {
        last_mic_state = mic_state;
        driver_set_mic(last_mic_state);
    }
    else if(last_mic_state != mic_state)
    {
        last_mic_state = mic_state;
        driver_set_mic(last_mic_state);
    }
}