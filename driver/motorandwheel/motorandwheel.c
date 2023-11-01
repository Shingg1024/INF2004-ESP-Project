#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

// Pin Definitions
#define L_IR_SENSOR_PIN 2   // Left wheel encoder
#define R_IR_SENSOR_PIN 5   // Right wheel encoder
#define LEFT_MOTOR_PWM 0    // The GPIO pin connected to the PWM motor 1
#define RIGHT_MOTOR_PWM 1   // The GPIO pin connected to the PWM motor 2
#define MOTOR_N1_PIN 16     // The GPIO pin connected to N1 of the L298N motor controller
#define MOTOR_N2_PIN 17     // The GPIO pin connected to N2 of the L298N motor controller
#define MOTOR_N3_PIN 18     // The GPIO pin connected to N3 of the L298N motor controller
#define MOTOR_N4_PIN 19     // The GPIO pin connected to N4 of the L298N motor controller

// Encoder-related variables
static uint32_t leftNotchCount = 0;
static uint32_t rightNotchCount = 0;
static absolute_time_t lastNotchTime;
static bool isCountingPulse = false;
const int dutyCycle = 1;

// Wheel characteristics
const float wheelCircumferenceMeters = 0.204; // Replace with your wheel's circumference
const int notchesPerRevolution = 20;

static float leftDistanceMeters = 0.0;
static float rightDistanceMeters = 0.0;
static float totalDistanceMeters = 0.0;
static char eventStr[128];

static const char *gpioIrqStr[] = {
    "LEVEL_LOW",  // 0x1
    "LEVEL_HIGH", // 0x2
    "EDGE_FALL",  // 0x4
    "EDGE_RISE"   // 0x8
};

void gpioEventString(char *buf, uint32_t events)
{
    for (uint i = 0; i < 4; i++)
    {
        uint mask = (1 << i);
        if (events & mask)
        {
            // Copy this event string into the user string
            const char *eventStr = gpioIrqStr[i];
            while (*eventStr != '\0')
            {
                *buf++ = *eventStr++;
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
    pwm_set_clkdiv(sliceNum1, 12500);
    pwm_set_wrap(sliceNum1, 3);
    pwm_set_chan_level(sliceNum1, PWM_CHAN_A, 12500 / dutyCycle);
    pwm_set_chan_level(sliceNum1, PWM_CHAN_B, 12500 / dutyCycle);

    pwm_set_clkdiv(sliceNum2, 12500);
    pwm_set_wrap(sliceNum2, 3);
    pwm_set_chan_level(sliceNum2, PWM_CHAN_A, 12500);
    pwm_set_chan_level(sliceNum2, PWM_CHAN_B, 12500);

    // Set the PWM running
    pwm_set_enabled(sliceNum1, true);
    pwm_set_enabled(sliceNum2, true);
}

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

int main()
{
    stdio_init_all();

    printf("Wheel Encoder Driver & PWM Motor Control\n");

    // Initialize GPIO pins for motor direction control
    gpio_init(MOTOR_N1_PIN);
    gpio_init(MOTOR_N2_PIN);
    gpio_init(MOTOR_N3_PIN);
    gpio_init(MOTOR_N4_PIN);
    gpio_set_dir(MOTOR_N1_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N2_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N3_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N4_PIN, GPIO_OUT);

    // Initialize PWM for the motors
    initializePwmForMotor(dutyCycle);

    // Set up the wheel encoder GPIO and callback
    gpio_set_irq_enabled_with_callback(L_IR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);
    // gpio_set_irq_enabled_with_callback(R_IR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE, true, &gpioCallback);

    while (1)
    {
        moveForward();
        // sleep_ms(5000);
        // moveBackward();
        // sleep_ms(5000);
        // moveRight();
        // sleep_ms(5000);
        // moveLeft(5000);

        // sleep_ms(2000);
        sleep_ms(1000);
        printf("Total Distance traveled: %.2f", totalDistanceMeters);
    }
    return 0; // Indicate successful execution
}
