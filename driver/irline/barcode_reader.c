#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

bool isBlackDetected = false;
bool isWhiteDetected = false;

uint32_t blackStartTime = 0;
uint32_t blackEndTime = 0;
uint32_t whiteStartTime = 0;
uint32_t whiteEndTime = 0;
uint32_t whiteWidth = 0;
uint32_t blackWidth = 0;

uint32_t totalBlackWidth = 0;
uint32_t totalWhiteWidth = 0;
// uint32_t totalOverallWidth = 0;

int overallCount = 0;

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
            if (!isBlackDetected)
            {
                if (isWhiteDetected)
                {
                    whiteEndTime = time_us_32();
                    whiteWidth = whiteEndTime - whiteStartTime;
                    // printf("White Width: %lu us\n", whiteWidth);

                    totalWhiteWidth += whiteWidth;
                    overallCount++;
                    // printf("Count: %d\n", overallCount);
                }
                blackStartTime = time_us_32();
                isBlackDetected = true;
                isWhiteDetected = false;
            }
        }
        else
        {
            // Detect white
            if (isBlackDetected)
            {
                blackEndTime = time_us_32();
                blackWidth = blackEndTime - blackStartTime;
                // printf("Black Width: %lu us\n", blackWidth);
                if (blackWidth > 100000)
                {
                    printf("Barcode is thick\n");
                }
                else
                {
                    printf("Barcode is thin\n");
                }
                totalBlackWidth += blackWidth;
                overallCount++;
                // printf("Count: %d\n", overallCount);
                if (overallCount >= 29)
                {
                    // printf("Total Black Width: %lu us\n", totalBlackWidth);
                    // printf("Total White Width: %lu us\n", totalWhiteWidth);
                    // printf("Total Overall Width: %lu us\n", (totalBlackWidth + totalWhiteWidth));

                    totalBlackWidth = 0;
                    totalWhiteWidth = 0;
                    overallCount = 0;
                }
                isBlackDetected = false;
                isWhiteDetected = true;
                whiteStartTime = time_us_32();
            }
        }
        sleep_ms(10);
    }
}