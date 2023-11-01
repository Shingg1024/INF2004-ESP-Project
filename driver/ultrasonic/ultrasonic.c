#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define TRIG_PIN 1
#define ECHO_PIN 3

int timeout = 26000;
volatile bool pulse_started = false;
volatile bool timeout_occurred = false;
volatile uint64_t width = 0;
absolute_time_t startTime;
absolute_time_t endTime;

void setupUltrasonicPins()
{
    gpio_init(TRIG_PIN);
    gpio_init(ECHO_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
}

// Efficiently remove block-waiting
void echo_callback(uint gpio, uint32_t events)
{
    if (gpio == ECHO_PIN)
    {
        if (gpio_get(ECHO_PIN) == 1)
        {
            startTime = get_absolute_time();
            pulse_started = true;
            timeout_occurred = false; // Reset timeout flag
        }
        else
        {
            endTime = get_absolute_time();
            pulse_started = false;
            width = absolute_time_diff_us(startTime, endTime);
        }
    }
}

float measure_distance()
{
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);

    // Wait for the pulse measurement to complete
    while (pulse_started)
    {
        tight_loop_contents();
    }

    sleep_us(1);
    if (width > timeout)
        return 0;

    uint64_t pulse_length = absolute_time_diff_us(startTime, endTime);

    // Speed of sound = 343 m/s
    // Distance = (Pulse * Speed of sound) / 2
    return pulse_length * 0.0343 / 2;
}

int main()
{
    stdio_init_all();
    setupUltrasonicPins();

    // Added interrupts for efficiency
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_callback);

    while (1)
    {
        float distance = measure_distance();
        printf("Distance: %.2f cm\n", distance);
        sleep_ms(1000);
    }

    return 0;
}
