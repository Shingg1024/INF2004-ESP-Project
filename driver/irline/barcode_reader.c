#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

char binaryString[49];

bool isLineDetected = false;
bool isGapDetected = false;

uint32_t startTime = 0;
uint32_t endTime = 0;
uint32_t gapStart = 0;
uint32_t gapEnd = 0;
uint32_t gapDetection = 0;
uint32_t pulseWidth = 0;

uint32_t totalPulseWidth = 0;
uint32_t totalGapWidth = 0;
uint32_t totalOverall = 0;

int pulseCount = 0;

// Function to read and convert ADC value
float readAndConvertADC()
{
    uint32_t result = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    return result * conversion_factor;
}

// Function to initialize GPIO and ADC
void initializeHardware()
{
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Set pin 26 to input mode
    gpio_set_function(26, GPIO_FUNC_SIO);
    gpio_disable_pulls(26);
    gpio_set_input_enabled(26, true);
}

int main()
{
    initializeHardware();
    while (1)
    {
        if (readAndConvertADC() > 1.5)
        {
            // Detect black
            if (!isLineDetected)
            {
                if (isGapDetected)
                {
                    gapEnd = time_us_32();
                    gapDetection = gapEnd - gapStart;
                    printf("Gap Width: %lu us\n", gapDetection);
                    totalGapWidth += gapDetection;
                    pulseCount++;
                    printf("Count: %d\n", pulseCount);
                }
                startTime = time_us_32();
                isLineDetected = true;
                isGapDetected = false;
            }
        }
        else
        {
            // Detect white
            if (isLineDetected)
            {
                endTime = time_us_32();
                pulseWidth = endTime - startTime;
                printf("Pulse Width: %lu us\n", pulseWidth);
                totalPulseWidth += pulseWidth;
                pulseCount++;
                printf("Count: %d\n", pulseCount);
                if (pulseCount == 29)
                {
                    printf("Total Pulse Width: %lu us\n", totalPulseWidth);
                    printf("Total Gap Width: %lu us\n", totalGapWidth);
                    printf("Total Width: %lu us\n",(totalPulseWidth+totalGapWidth));
                }
                isLineDetected = false;
                isGapDetected = true;
                gapStart = time_us_32();
            }
        }
        sleep_ms(10);
    }
}