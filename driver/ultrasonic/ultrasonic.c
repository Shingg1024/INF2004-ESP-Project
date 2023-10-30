#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define TRIG_PIN 1
#define ECHO_PIN 3

int timeout = 32000;

void setupUltrasonicPins()
{
    gpio_init(TRIG_PIN);
    gpio_init(ECHO_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
}

float measure_distance()
{
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);

    absolute_time_t startTime;
    absolute_time_t endTime;

    uint64_t width = 0;

    while (gpio_get(ECHO_PIN) == 0)
    {
        startTime = get_absolute_time();
    }

    while (gpio_get(ECHO_PIN) == 1)
    {

        endTime = get_absolute_time();
        width++;
        sleep_us(1);
        if (width > timeout)
            return 0;
    }

    uint64_t pulseLength = absolute_time_diff_us(startTime, endTime);

    // Speed of sound = 343ms
    // Distance = (Pulse * Speed of sound) / 2
    return pulseLength * 0.0343 / 2; 
}

int main()
{
    stdio_init_all();
    setupUltrasonicPins();

    while (1)
    {
        float distance = measure_distance();
        printf("Distance: %.2f cm\n", distance);
        sleep_ms(1000);
    }

    return 0;
}