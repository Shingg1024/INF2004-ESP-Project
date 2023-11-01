#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Pin Definitions
#define L_IR_SENSOR_PIN 2   // Left wheel encoder
#define R_IR_SENSOR_PIN 5   // Right wheel encoder


static char eventStr[128];
void gpio_event_string(char *buf, uint32_t events);

// Encoder-related variables
static uint32_t leftNotchCount = 0;
static uint32_t rightNotchCount = 0;
static absolute_time_t lastNotchTime;
static bool isCountingPulse = false;
static const int dutyCycle = 1;

// Wheel characteristics
static const float wheelCircumferenceMeters = 0.204; // Replace with your wheel's circumference
static uint32_t notchesPerRevolution = 20;
static float leftDistanceMeters = 0.0;
static float rightDistanceMeters = 0.0;
static float totalDistanceMeters = 0.0;


void gpio_event_string(char *buf, uint32_t events) {
    for (uint i = 0; i < 4; i++) {
        uint mask = (1 << i);
        if (events & mask) {
            // Copy this event string into the user string
            const char *event_str = gpio_irq_str[i];
            while (*event_str != '\0') {
                *buf++ = *event_str++;
            }
            events &= ~mask;

            // If more events add ", "
            if (events) {
                *buf++ = ',';
                *buf++ = ' ';
            }
        }
    }
    *buf++ = '\0';
}

static const char *gpio_irq_str[] = {
    "LEVEL_LOW",  // 0x1
    "LEVEL_HIGH", // 0x2
    "EDGE_FALL",  // 0x4
    "EDGE_RISE"   // 0x8
};

// Wheel encoder GPIO interrupt
void gpioCallback(uint gpio, uint32_t events)
{
    // Put the GPIO event(s) that just happened into eventStr
    gpioEventString(eventStr, events);

    if (gpio == L_IR_SENSOR_PIN)
    {
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            // Edge-triggered, rising edge detected
            if (!isCountingPulse)
            {
                // Start counting the pulse
                lastNotchTime = get_absolute_time();
                isCountingPulse = true;
            }
        }
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            // Edge-triggered, falling edge detected
            if (isCountingPulse)
            {
                // Stop counting the pulse and calculate the width
                absolute_time_t currentTime = get_absolute_time();
                uint32_t pulseWidthUs = absolute_time_diff_us(lastNotchTime, currentTime);

                // Calculate speed in meters per second
                float speedMps = (wheelCircumferenceMeters / notchesPerRevolution) / (pulseWidthUs * 1e-6);
                float speedKmph = speedMps * 3.6;

                // Increment the notch count
                leftNotchCount++;

                // Calculate distance based on the notch count
                leftDistanceMeters = (wheelCircumferenceMeters / notchesPerRevolution) * leftNotchCount;

                // Update the total distance as the sum of left and right distances
                totalDistanceMeters = leftDistanceMeters + rightDistanceMeters;

                printf("Left Motor Speed: %.2f km/h\n", speedKmph);

                isCountingPulse = false;
            }
        }
    }

    else if (gpio == R_IR_SENSOR_PIN)
    {
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            // Edge-triggered, rising edge detected
            if (!isCountingPulse)
            {
                // Start counting the pulse
                lastNotchTime = get_absolute_time();
                isCountingPulse = true;
            }
        }
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            // Edge-triggered, falling edge detected
            if (isCountingPulse)
            {
                // Stop counting the pulse and calculate the width
                absolute_time_t currentTime = get_absolute_time();
                uint32_t pulseWidthUs = absolute_time_diff_us(lastNotchTime, currentTime);

                // Calculate speed in meters per second
                float speedMps = (wheelCircumferenceMeters / notchesPerRevolution) / (pulseWidthUs * 1e-6);
                float speedKmph = speedMps * 3.6;

                // Increment the notch count
                rightNotchCount++;

                // Calculate distance based on the notch count
                rightDistanceMeters = (wheelCircumferenceMeters / notchesPerRevolution) * rightNotchCount;

                // Update the total distance as the sum of left and right distances
                totalDistanceMeters = leftDistanceMeters + rightDistanceMeters;

                printf("Right Motor Speed: %.2f km/h\n", speedKmph);

                isCountingPulse = false;
            }
        }
    }
}


int main()
{
    stdio_init_all();
    sleep_ms(1000);
    printf("Wheel Encoder Driver\n");


    // Set up the wheel encoder GPIO and callback
    gpio_set_irq_enabled_with_callback(L_IR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);
    gpio_set_irq_enabled_with_callback(R_IR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);

    while (1){
            printf("Total Distance traveled: %.2f", totalDistanceMeters);
    }


}

