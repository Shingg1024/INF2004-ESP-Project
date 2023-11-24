#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"

int test = 0;

bool isLeftBlack = false;
bool isLeftWhite = false;
bool isRightBlack = false;
bool isRightWhite = false;
bool testChecker = false;

// Pin Definitions
#define L_IR_SENSOR_PIN 2 // Left wheel encoder
#define R_IR_SENSOR_PIN 5 // Right wheel encoder
#define LEFT_MOTOR_PWM 0  // The GPIO pin connected to the PWM motor 1
#define RIGHT_MOTOR_PWM 1 // The GPIO pin connected to the PWM motor 2
#define MOTOR_N1_PIN 16   // The GPIO pin connected to N1 of the L298N motor controller
#define MOTOR_N2_PIN 17   // The GPIO pin connected to N2 of the L298N motor controller
#define MOTOR_N3_PIN 18   // The GPIO pin connected to N3 of the L298N motor controller
#define MOTOR_N4_PIN 19   // The GPIO pin connected to N4 of the L298N motor controller

// code from ir_line_sensor
#define barcodePIN 26
#define L_IR_LINE_SENSOR 27
#define R_IR_LINE_SENSOR 28

// code from ultrasonic
#define TRIG_PIN 10
#define ECHO_PIN 13

#define HMC5883L_ADDR 0x1E
#define CONFIG_REG_A 0x00
#define MODE_REG 0x02
#define DATA_OUTPUT_X_MSB 0x03

// Encoder-related variables
static uint32_t NotchCount = 0;
static absolute_time_t lastNotchTime;
static bool isCountingPulse = false;
const int dutyCycle = 10;

// Wheel characteristics
static const float wheelCircumferenceMeters = 0.204; // Replace with your wheel's circumference
static const uint32_t notchesPerRevolution = 20;
static float totalDistanceMeters = 0.0;

// code from ir_line_sensor(barcode)
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

// code from ultrasonic
int timeout = 26000;
volatile bool pulse_started = false;
volatile bool timeout_occurred = false;
volatile uint64_t width = 0;
absolute_time_t startTime;
absolute_time_t endTime;

// Function to configure and start PWM for the motor
void initializePwmForMotor(int dutyCycle)
{
    // Set up PWM for the motors
    gpio_set_function(LEFT_MOTOR_PWM, GPIO_FUNC_PWM);
    gpio_set_function(RIGHT_MOTOR_PWM, GPIO_FUNC_PWM);

    // Find out which PWM slices are connected to the GPIO pins
    uint sliceNum1 = pwm_gpio_to_slice_num(LEFT_MOTOR_PWM);
    uint sliceNum2 = pwm_gpio_to_slice_num(RIGHT_MOTOR_PWM);

    // Configure PWM settings as needed for your motors
    pwm_set_clkdiv(sliceNum1, 100);
    pwm_set_wrap(sliceNum1, 12500);
    pwm_set_chan_level(sliceNum1, PWM_CHAN_A, 7000);
    pwm_set_chan_level(sliceNum1, PWM_CHAN_B, 7000);

    pwm_set_clkdiv(sliceNum2, 100);
    pwm_set_wrap(sliceNum2, 12500);
    pwm_set_chan_level(sliceNum2, PWM_CHAN_A, 7000);
    pwm_set_chan_level(sliceNum2, PWM_CHAN_B, 7000);

    // Set the PWM running
    pwm_set_enabled(sliceNum1, true);
    pwm_set_enabled(sliceNum2, true);
}

// Function to set the motor direction
void setMotorDirection(bool n1, bool n2, bool n3, bool n4)
{
    gpio_put(MOTOR_N1_PIN, n1 ? 1 : 0);
    gpio_put(MOTOR_N2_PIN, n2 ? 1 : 0);
    gpio_put(MOTOR_N3_PIN, n3 ? 1 : 0);
    gpio_put(MOTOR_N4_PIN, n4 ? 1 : 0);
}

// Function to stop the motor
void stopMotor()
{
    setMotorDirection(0, 0, 0, 0);
}

// Function to move the motor forward
void moveForward()
{
    setMotorDirection(1, 0, 1, 0);
}

// Function to move the motor backward
void moveBackward()
{
    setMotorDirection(0, 1, 0, 1);
}

// Function to move the motor left
void moveLeft()
{
    setMotorDirection(0, 1, 1, 0);
}

// Function to move the motor right
void moveRight()
{
    setMotorDirection(1, 0, 0, 1);
}

void gpio_event_string(char *buf, uint32_t events);

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

// Function to read and convert ADC value
float readAndConvertADC()
{
    uint32_t result = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    return result * conversion_factor;
}

// Callback function for Barcode events
void gpioCallback(uint gpio, uint32_t events)
{
    gpio_event_string(event_str, events);

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

    if (gpio == L_IR_SENSOR_PIN || gpio == R_IR_SENSOR_PIN)
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
            else
            {

                // Update total distance based on both wheels
                totalDistanceMeters = (wheelCircumferenceMeters / notchesPerRevolution) * NotchCount;

                // Stop counting the pulse and calculate the width
                absolute_time_t currentTime = get_absolute_time();
                uint32_t pulseWidthUs = absolute_time_diff_us(lastNotchTime, currentTime);

                // Calculate speed in meters per second
                float speedMps = (wheelCircumferenceMeters / notchesPerRevolution) / (pulseWidthUs * 1e-6);
                float speedKmph = speedMps * 3.6;

                // Print speed for the corresponding wheel
                if (gpio == L_IR_SENSOR_PIN)
                {
                    printf("Left Motor km/hr: %.2f\n", speedKmph);
                }
                else if (gpio == R_IR_SENSOR_PIN)
                {
                    printf("Right Motor km/hr: %.2f\n", speedKmph);
                }

                isCountingPulse = false;
            }
        }
    }

    if (gpio == barcodePIN)
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

    if (gpio == L_IR_LINE_SENSOR)
    {
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            isLeftBlack = true;
        }
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            isLeftWhite = true;
        }
    }
    if (gpio == R_IR_LINE_SENSOR)
    {
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            isRightBlack = true;
        }
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            isRightWhite = true;
        }
    }
}

// Initialize hardware components
void initHardware()
{
    stdio_init_all();
    adc_init();

    gpio_init(barcodePIN);
    gpio_disable_pulls(barcodePIN);

    gpio_init(L_IR_LINE_SENSOR);
    gpio_disable_pulls(L_IR_LINE_SENSOR);

    gpio_init(R_IR_LINE_SENSOR);
    gpio_disable_pulls(R_IR_LINE_SENSOR);

    gpio_init(MOTOR_N1_PIN);
    gpio_init(MOTOR_N2_PIN);
    gpio_init(MOTOR_N3_PIN);
    gpio_init(MOTOR_N4_PIN);
    gpio_set_dir(MOTOR_N1_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N2_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N3_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N4_PIN, GPIO_OUT);

    gpio_init(TRIG_PIN);
    gpio_init(ECHO_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    // Added interrupts for efficiency
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);

    // init interrupt
    gpio_set_irq_enabled_with_callback(L_IR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);

    gpio_set_irq_enabled_with_callback(barcodePIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);
    gpio_set_irq_enabled_with_callback(L_IR_LINE_SENSOR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);
    gpio_set_irq_enabled_with_callback(R_IR_LINE_SENSOR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);
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

void hmc5883l_init(i2c_inst_t *i2c)
{
    uint8_t init_data[2] = {CONFIG_REG_A, 0x70}; // Set to continuous measurement mode
    i2c_write_blocking(i2c, HMC5883L_ADDR, init_data, 2, false);

    uint8_t mode_data[2] = {MODE_REG, 0x00}; // Set to continuous measurement mode
    i2c_write_blocking(i2c, HMC5883L_ADDR, mode_data, 2, false);
}

void read_magnetic_field(i2c_inst_t *i2c, int16_t *mag_data)
{
    uint8_t reg = DATA_OUTPUT_X_MSB;
    uint8_t data[6];

    i2c_write_blocking(i2c, HMC5883L_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c, HMC5883L_ADDR, data, 6, false);

    for (int i = 0; i < 3; i++)
    {
        mag_data[i] = (data[i * 2] << 8) | data[i * 2 + 1];
    }
}

float calculate_heading(int16_t x, int16_t y)
{
    float heading = atan2(y, x) * (180.0 / M_PI);
    if (heading < 0)
    {
        heading += 360.0;
    }
    return heading;
}

int main()
{
    initHardware();

    // Initialize PWM for the motors
    initializePwmForMotor(dutyCycle);

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
        if (isLeftBlack)
        {
            // moveRight();
            // sleep_ms(1000);
            // stopMotor();
            // sleep_ms(1000);
            // stopMotor();
            setMotorDirection(1, 0, 0, 0);
            sleep_ms(500);
            isLeftBlack = false;
        }

        if (isLeftWhite)
        {
            moveForward();
            isLeftWhite = false;
        }

        if (isRightBlack)
        {
            // moveRight();
            // sleep_ms(1000);
            // stopMotor();
            // sleep_ms(1000);
            // stopMotor();
            // Find out which PWM slices are connected to the GPIO pins
            uint sliceNum1 = pwm_gpio_to_slice_num(LEFT_MOTOR_PWM);
            uint sliceNum2 = pwm_gpio_to_slice_num(RIGHT_MOTOR_PWM);

            // Configure PWM settings as needed for your motors
            pwm_set_clkdiv(sliceNum1, 100);
            pwm_set_wrap(sliceNum1, 12500);
            pwm_set_chan_level(sliceNum1, PWM_CHAN_A, 7500);
            pwm_set_chan_level(sliceNum1, PWM_CHAN_B, 7500);

            pwm_set_clkdiv(sliceNum2, 100);
            pwm_set_wrap(sliceNum2, 12500);
            pwm_set_chan_level(sliceNum2, PWM_CHAN_A, 7500);
            pwm_set_chan_level(sliceNum2, PWM_CHAN_B, 7500);
            setMotorDirection(0, 0, 1, 0);
            sleep_ms(500);
            isRightBlack = false;
        }

        if (isRightWhite)
        {
            moveForward();
            isRightWhite = false;
        }
        // float distance = measure_distance();
        // printf("Distance: %.2f cm\n", distance);

        // if (distance < 10)
        // {
        //     stopMotor();
        //     sleep_ms(3000);
        //     uint sliceNum1 = pwm_gpio_to_slice_num(LEFT_MOTOR_PWM);
        //     uint sliceNum2 = pwm_gpio_to_slice_num(RIGHT_MOTOR_PWM);

        //     // Configure PWM settings as needed for your motors
        //     pwm_set_clkdiv(sliceNum1, 100);
        //     pwm_set_wrap(sliceNum1, 12500);
        //     pwm_set_chan_level(sliceNum1, PWM_CHAN_A, 12500);
        //     pwm_set_chan_level(sliceNum1, PWM_CHAN_B, 12500);

        //     pwm_set_clkdiv(sliceNum2, 100);
        //     pwm_set_wrap(sliceNum2, 12500);
        //     pwm_set_chan_level(sliceNum2, PWM_CHAN_A, 12500);
        //     pwm_set_chan_level(sliceNum2, PWM_CHAN_B, 12500);
        //     moveBackward();
        //     sleep_ms(3000);
        //     stopMotor();

        //     main();
        // }
        // else
        // {
        //     uint sliceNum1 = pwm_gpio_to_slice_num(LEFT_MOTOR_PWM);
        //     uint sliceNum2 = pwm_gpio_to_slice_num(RIGHT_MOTOR_PWM);

        //     // Configure PWM settings as needed for your motors
        //     pwm_set_clkdiv(sliceNum1, 100);
        //     pwm_set_wrap(sliceNum1, 12500);
        //     pwm_set_chan_level(sliceNum1, PWM_CHAN_A, 12500);
        //     pwm_set_chan_level(sliceNum1, PWM_CHAN_B, 12500);

        //     pwm_set_clkdiv(sliceNum2, 100);
        //     pwm_set_wrap(sliceNum2, 12500);
        //     pwm_set_chan_level(sliceNum2, PWM_CHAN_A, 12500);
        //     pwm_set_chan_level(sliceNum2, PWM_CHAN_B, 12500);
        //     moveForward();

        // }

        // moveForward();

        // motor part.
        // printf("Total Distance traveled: %.2f", totalDistanceMeters);
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