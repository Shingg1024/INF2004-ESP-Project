#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

uint64_t startTimeBlack;
uint64_t endTimeBlack;
uint64_t startTimeWhite;
uint64_t endTimeWhite;
uint64_t startTimeBlackLine;
uint64_t endTimeBlackLine;

volatile uint64_t blackWidth = 0;
volatile uint64_t whiteWidth = 0;
volatile uint64_t blackLine = 0;

bool isRise = false;
bool isFail = false;
bool isGap = false;

static char event_str[128];
char binaryString[49];
int binaryTest[28];

int count = 0;
float totalAvg = 0;
int j_counter = 0;

float testAvg = 0;
bool isReverse = false;

// for interrupt

// struct for dictionary
struct KeyValuePair
{
    char key;
    const char *value;
    const char *reverse;
};

// Searching from array to find matches
void findAndPrintMatches(const char *input, struct KeyValuePair dictionary[], int dictionarySize)
{
    int length = strlen(input);

    for (int i = 0; i < length; i += 16)
    {
        char segment[17]; // A segment plus the null-terminator
        strncpy(segment, input + i, 16);
        segment[16] = '\0'; // Null-terminate the segment
        // Compare the segment with dictionary entries
        for (int j = 0; j < dictionarySize; j++)
        {
            if (strcmp(segment, dictionary[j].value) == 0 || strcmp(segment, dictionary[j].reverse) == 0)
            {
                printf("%c ", dictionary[j].key);
            }
        }
    }
}

void gpio_event_string(char *buf, uint32_t events);

// Function to read and convert ADC value
float readAndConvertADC()
{
    uint32_t result = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    return result * conversion_factor;
}

// Initialize hardware components
void initHardware()
{
    stdio_init_all();
    adc_init();

    gpio_init(26);
    gpio_disable_pulls(26);

    gpio_init(27);
    gpio_disable_pulls(27);

    gpio_init(28);
    gpio_disable_pulls(28);
}

// Callback function for Barcode events
void gpio_callback(uint gpio, uint32_t events)
{
    gpio_event_string(event_str, events);

    if (gpio == 26)
    {
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            isRise = true;
        }
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            isFail = true;
        }
    }
    else if (gpio == 27)
    {
        // 27 for left
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            startTimeBlackLine = to_ms_since_boot(get_absolute_time());
        }
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            endTimeBlackLine = to_ms_since_boot(get_absolute_time());
            blackLine = endTimeBlackLine - startTimeBlackLine;
            printf("%d Black line: %llu long\n", gpio, blackLine);
        }
    }
    else if (gpio == 28)
    {
        // 28 for right
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            startTimeBlackLine = to_ms_since_boot(get_absolute_time());
        }
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            endTimeBlackLine = to_ms_since_boot(get_absolute_time());
            blackLine = endTimeBlackLine - startTimeBlackLine;
            printf("%d Black line: %llu long\n", gpio, blackLine);
        }
    }
}

void checkReverse(int width[], int count)
{
    for (int i = 0; i < count; i++)
    {
        testAvg += width[i];
    }
    // printf("Total: %.1f\n", testAvg);
    testAvg /= count;
    // printf("Avg : %.1f\n", testAvg);

    if (width[2] > testAvg)
    {
        isReverse = true;
    }
}

float getTotalAverage(int width[], int count)
{
    float avg = 0;
    for (int i = 0; i < count; i++)
    {
        avg += width[i];
    }
    // printf("Total: %.1f\n", avg);
    avg /= count;
    // printf("Avgerage: %.1f");
    return avg;
}

void resetEverything()
{
    isRise = false;
    isFail = false;
    isGap = false;
    isReverse = false;

    count = 0;
    j_counter = 0;
    totalAvg = 0;
    testAvg = 0;

    // Empty the array by setting all elements to '\0'
    memset(binaryString, '\0', sizeof(binaryString));
    // Empty the array by setting all elements to '0'
    memset(binaryTest, '\0', sizeof(binaryTest));
}

int main()
{
    initHardware();
    // init interrupt
    gpio_set_irq_enabled_with_callback(26, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(27, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(28, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Dictionary for encoding characters
    static struct KeyValuePair dictionary[] = {
        {'A', "1110101000101110", "0111010001010111"},
        {'B', "1011101000101110", "0111010001011101"},
        {'C', "1110111010001010", "0101000101110111"},
        {'D', "1010111000101110", "0111010001110101"},
        {'E', "1110101110001010", "0101000111010111"},
        {'F', "1011101110001010", "0101000111011101"},
        {'G', "1010100011101110", "0111011100010101"},
        {'H', "1110101000111010", "0101110001010111"},
        {'I', "1011101000111010", "0101110001011101"},
        {'J', "1010111000111010", "0101110001110101"},
        {'K', "1110101010001110", "0111000101010111"},
        {'L', "1011101010001110", "0111000101011101"},
        {'M', "1110111010100010", "0100010101110111"},
        {'N', "1010111010001110", "0111000101110101"},
        {'O', "1110101110100010", "0100010111010111"},
        {'P', "1011101110100010", "0100010111011101"},
        {'Q', "1010101110001110", "0111000111010101"},
        {'R', "1110101011100010", "0100011101010111"},
        {'S', "1011101011100010", "0100011101011101"},
        {'T', "1010111011100010", "0100011101110101"},
        {'U', "1110001010101110", "0111010101000111"},
        {'V', "1000111010101110", "0111010101110001"},
        {'W', "1110001110101010", "0101010111000111"},
        {'X', "1000101110101110", "0111010111010001"},
        {'Y', "1110001011101010", "0101011101000111"},
        {'Z', "1000111011101010", "0101011101110001"},
        {'0', "1010001110111010", "0101110111000101"},
        {'1', "1110100010101110", "0111010100010111"},
        {'2', "1011100010101110", "0111010100011101"},
        {'3', "1110111000101010", "0101010001110111"},
        {'4', "1010001110101110", "0111010111000101"},
        {'5', "1110100011101010", "0101011100010111"},
        {'6', "1011100011101010", "0101011100011101"},
        {'7', "1010001011101110", "0111011101000101"},
        {'8', "1110100010111010", "0101110100010111"},
        {'9', "1011100010111010", "0101110100011101"},
        {'*', "1000101110111010", "0101110111010001"},
        {' ', "1000111010111010", "0101110101110001"},
        {'-', "1000101011101110", "0111011101010001"},
        {'$', "1000100010001010", "0101000100010001"},
        {'%', "1010001000100010", "0100010001000101"},
        {'.', "1110001010111010", "0101110101000111"},
        {'/', "1000100010100010", "0100010100010001"},
        {'+', "1000101000100010", "0100010001010001"}};

    // Wait forever
    while (1)
    {
        // Edge Rise is detected
        if (isRise)
        {
            if (isGap)
            {
                // end the time when detect black.
                endTimeWhite = to_ms_since_boot(get_absolute_time());
                // getting the gap width value
                whiteWidth = endTimeWhite - startTimeWhite;

                if (whiteWidth > 1000)
                {
                    printf("Too long, reset all to default\n");
                    resetEverything();
                }
                else
                {
                    // display output for the gap (Debug purpose)
                    // printf("White Width: %llu\n", whiteWidth);
                    // add the width value to array
                    binaryTest[count] = whiteWidth;
                    // counter for the arrayWidth
                    count++;
                    // display the output of the couter (Debug purpose)
                    printf("Count: %d\n", count);
                }

                // set the gap condition to false.
                isGap = false;
            }
            // Start a timer when detect black.
            startTimeBlack = to_ms_since_boot(get_absolute_time());
            // Set the rise to false once done
            isRise = false;
        }
        if (isFail)
        {
            // Stop the timer when detect white
            endTimeBlack = to_ms_since_boot(get_absolute_time());
            // check the different for the width
            blackWidth = endTimeBlack - startTimeBlack;
            // display the output of the black bar width (Debug purpose)
            // printf("Black Width: %llu \n", blackWidth);
            // add the black bar width into a array
            binaryTest[count] = blackWidth;
            // counter for the array
            count++;
            // display the counter output (Debug purpose)
            printf("Count: %d\n", count);

            if (count == 9)
            {
                checkReverse(binaryTest, count);
            }

            // when the count reached 29
            if (count == 29)
            {
                // show the scan is done (Debug purpose)
                // printf("Scan done\n");

                if (!isReverse)
                {
                    // // for loop to add all the value into 'totalAvg'
                    // for (size_t i = 0; i < count; i++)
                    // {
                    //     totalAvg += binaryTest[i];
                    // }
                    // // Display the total of the array value first before doing the avgerage (Debug Purpose)
                    // printf("Total: %.1f\n", totalAvg);
                    // // step to do average
                    // totalAvg = totalAvg / count;
                    // // Display the average value (Debug Purpose)
                    // printf("Avg: %.1f\n", totalAvg);

                    totalAvg = getTotalAverage(binaryTest, count);

                    // for loop to get all the value (Debug Purpose)
                    // for (size_t i = 0; i < count; i++)
                    // {
                    //     printf("%d\n", binaryTest[i]);
                    // }

                    // hard coded first and last to 0 as thin
                    binaryTest[0] = 0;
                    binaryTest[28] = 0;

                    // for loop from the width array
                    for (int i = 0; i < count; i++)
                    {
                        // checking the width value is greater than total avg is true
                        // if true mean is thick
                        if (binaryTest[i] > totalAvg)
                        {
                            // checking is black or white
                            // %2 == 0 mean is black
                            if (i % 2 == 0)
                            {
                                // hard coded for loop 3 time to add 3 '1' to new char array
                                for (int j = 0; j < 3; j++)
                                {
                                    binaryString[j_counter] = '1';
                                    j_counter++;
                                }
                                // display which thick (Debug Purpose)
                                // printf("Black Thick\n");
                                // %2 == 1 mean is white
                            }
                            else if (i % 2 == 1)
                            {
                                // hard coded for loop 3 time to add 3 '0' to new char array
                                for (int j = 0; j < 3; j++)
                                {
                                    binaryString[j_counter] = '0';
                                    j_counter++;
                                }
                                // display which thick (Debug Purpose)
                                // printf("White Thick\n");
                            }
                            // printf("Thick\n");
                        }
                        // false mean is thin
                        else
                        {
                            // checking is black or white
                            // %2 == 0 mean is black
                            if (i % 2 == 0)
                            {
                                // add '1' to new char array
                                binaryString[j_counter] = '1';
                                j_counter++;
                                // display value (Debug Purpose)
                                // printf("Black Thin\n");
                                // %2 == 1 mean is white
                            }
                            else if (i % 2 == 1)
                            {
                                // add '0' to new char array
                                binaryString[j_counter] = '0';
                                j_counter++;
                                // display value (Debug Purpose)
                                // printf("White Thin\n");
                            }
                            // printf("Thin\n");
                        }
                    }

                    // hard code last 0 as is at the white area and is cfm '0' for the last.
                    binaryString[47] = '0';
                    // add counter + 1
                    j_counter++;
                    // hard code last char string value as '\0' to prevent null pointer error.
                    binaryString[48] = '\0';
                    // add counter + 1
                    j_counter++;

                    // display final count (Debug Purpose)
                    // printf("Final Counter: %d\n", j_counter);
                    // display final count (Debug Purpose)
                    printf("Value: %s\n", binaryString);
                    // findAndPrintMatches will split the array into size 16 and check the value match the dictionary value
                    findAndPrintMatches(binaryString, dictionary, sizeof(dictionary) / sizeof(dictionary[0]));
                    // display new line after the matches
                    printf("\n");

                    resetEverything();
                }
                else
                {
                    // printf("is reverse, add code here\n");
                    for (int i = 0; i < count; i++)
                    {
                        totalAvg += binaryTest[i];
                    }
                    // printf("Sum total: %.1f\n", totalAvg);
                    totalAvg = totalAvg / count;
                    // printf("Avg total: %.1f\n", totalAvg);
                    binaryTest[0] = 0;
                    binaryString[0] = '0';
                    j_counter++;
                    for (int i = 0; i < count; i++)
                    {
                        if (binaryTest[i] > totalAvg)
                        {
                            if (i % 2 == 0)
                            {
                                for (int j = 0; j < 3; j++)
                                {
                                    binaryString[j_counter] = '1';
                                    j_counter++;
                                }
                            }
                            else if (i % 2 == 1)
                            {
                                for (int j = 0; j < 3; j++)
                                {
                                    binaryString[j_counter] = '0';
                                    j_counter++;
                                }
                            }
                        }
                        else
                        {
                            if (i % 2 == 0)
                            {
                                binaryString[j_counter] = '1';
                                j_counter++;
                            }
                            else if (i % 2 == 1)
                            {
                                binaryString[j_counter] = '0';
                                j_counter++;
                            }
                        }
                    }
                    // hard code last char string value as '\0' to prevent null pointer error.
                    binaryString[48] = '\0';
                    // add counter + 1
                    j_counter++;
                    // display final count (Debug Purpose)
                    // printf("Final Counter: %d\n", j_counter);
                    // display final count (Debug Purpose)
                    printf("Value: %s\n", binaryString);
                    // findAndPrintMatches will split the array into size 16 and check the value match the dictionary value
                    findAndPrintMatches(binaryString, dictionary, sizeof(dictionary) / sizeof(dictionary[0]));
                    // display new line after the matches
                    // printf("\n");

                    resetEverything();
                }
            }

            // start the time for the gap
            startTimeWhite = to_ms_since_boot(get_absolute_time());
            // set the gap to true
            isGap = true;
            // set fail is false to prevent inf loop.
            isFail = false;
        }
    }
}

// Function to create a string representing GPIO events
static const char *gpio_irq_str[] = {
    "LEVEL_LOW",  // 0x1
    "LEVEL_HIGH", // 0x2
    "EDGE_FALL",  // 0x4
    "EDGE_RISE"   // 0x8
};

void gpio_event_string(char *buf, uint32_t events)
{
    for (uint i = 0; i < 4; i++)
    {
        uint mask = (1 << i);
        if (events & mask)
        {
            // Copy this event string into the user string
            const char *event_str = gpio_irq_str[i];
            while (*event_str != '\0')
            {
                *buf++ = *event_str++;
            }
            events &= ~mask;

            // If more events add ", "
            if (events)
            {
                *buf++ = ',';
                *buf++ = ' ';
            }
        }
    }
    *buf++ = '\0';
}