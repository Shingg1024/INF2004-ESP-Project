#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#define LEFT_MOTOR_PWM 0  // The GPIO pin connected to the PWM motor 1
#define RIGHT_MOTOR_PWM 1 // The GPIO pin connected to the PWM motor 2
#define MOTOR_N1_PIN 16   // The GPIO pin connected to N1 of the L298N motor controller
#define MOTOR_N2_PIN 17   // The GPIO pin connected to N2 of the L298N motor controller
#define MOTOR_N3_PIN 18   // The GPIO pin connected to N3 of the L298N motor controller
#define MOTOR_N4_PIN 19   // The GPIO pin connected to N4 of the L298N motor controller

static const uint32_t dutyCycle = 1;

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

    printf("PWM Motor Control\n");

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
    while (1)
    {
        moveForward();
        sleep_ms(5000);
        moveBackward();
        sleep_ms(5000);
        moveRight();
        sleep_ms(5000);
        moveLeft(5000);
        sleep_ms(5000);
    }
    return 0;  // Indicate successful execution
}
