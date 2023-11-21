// Include necessary libraries
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
// Defining constants
#define HMC5883L_ADDR 0x1E
#define HMC5883L_CONFIG_REG_A 0x00
#define HMC5883L_MODE_REG 0x02
#define HMC5883L_DATA_OUTPUT_X_MSB 0x03
#define LSM303_ACC_ADDR 0x19
#define LSM303_CTRL_REG1_A 0x20
#define LSM303_CTRL_REG4_A 0x23
#define LSM303_OUT_X_L_A 0x28

// Initialise the HMC5883L sensor
void hmc5883l_init(i2c_inst_t *i2c) {
    uint8_t init_data[2] = {HMC5883L_CONFIG_REG_A, 0x70}; // Set to continuous measurement mode
    i2c_write_blocking(i2c, HMC5883L_ADDR, init_data, 2, false);

    uint8_t mode_data[2] = {HMC5883L_MODE_REG, 0x00}; // Set to continuous measurement mode
    i2c_write_blocking(i2c, HMC5883L_ADDR, mode_data, 2, false);
}
// Read magnetic field
void read_magnetic_field(i2c_inst_t *i2c, int16_t *mag_data) {
    uint8_t reg = HMC5883L_DATA_OUTPUT_X_MSB;
    uint8_t data[6];

    i2c_write_blocking(i2c, HMC5883L_ADDR, &reg, 1, true); // Sends request to read data
    i2c_read_blocking(i2c, HMC5883L_ADDR, data, 6, false); // Reads 6 bytes of data, 2 bytes for each axis

    for (int i = 0; i < 3; i++) {
        mag_data[i] = (data[i * 2] << 8) | data[i * 2 + 1]; // Stores in array
    }
}
// Calculate heading in degrees using X, Y components and arctangent function
float calculate_heading(int16_t x, int16_t y) {
    float heading = atan2(y, x) * (180.0 / M_PI);
    if (heading < 0) {
        heading += 360.0;
    }
    return heading;
}

// Intialise accelerometer sensor
void lsm303_init(i2c_inst_t *i2c) {
    uint8_t ctrl_reg1_data[2] = {LSM303_CTRL_REG1_A, 0x27}; // Enable accelerometer
    i2c_write_blocking(i2c, LSM303_ACC_ADDR, ctrl_reg1_data, 2, false);

    uint8_t ctrl_reg4_data[2] = {LSM303_CTRL_REG4_A, 0x00}; // Set scale to +/- 2g
    i2c_write_blocking(i2c, LSM303_ACC_ADDR, ctrl_reg4_data, 2, false);
}
// Read acceleration
void read_acceleration(i2c_inst_t *i2c, int16_t *acc_data) {
    uint8_t reg = LSM303_OUT_X_L_A | 0x80; // Auto-increment register address
    uint8_t data[6];

    i2c_write_blocking(i2c, LSM303_ACC_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c, LSM303_ACC_ADDR, data, 6, false);

    for (int i = 0; i < 3; i++) {
        acc_data[i] = (int16_t)((data[2 * i + 1] << 8) | data[2 * i]);
    }
}


int main() {
    stdio_init_all();

    // Initialise I2C
    i2c_inst_t *i2c = i2c0;
    i2c_init(i2c, 100000);
    gpio_set_function(0, GPIO_FUNC_I2C); // Assign I2C pins (SDA)
    gpio_set_function(1, GPIO_FUNC_I2C); // Assign I2C pins (SCL)

    // Intitialise the HMC5883L sensor
    hmc5883l_init(i2c);
    lsm303_init(i2c);

    int16_t magnetic_data[3];
    int16_t accel_data[3];

    while (1) {
        // Read magnetic field data
        read_magnetic_field(i2c, magnetic_data);
        read_acceleration(i2c, accel_data);

        int16_t mag_x = magnetic_data[0];
        int16_t mag_y = magnetic_data[1];
        int16_t mag_z = magnetic_data[2];

        int16_t acc_x = accel_data[0];
        int16_t acc_y = accel_data[1];
        int16_t acc_z = accel_data[2];

        // Calculate and print heading
        float heading = calculate_heading(mag_x, mag_y);
        printf("Heading: %.2f degrees\n", heading);
        printf("Magnetic Field X: %d, Y: %d, Z: %d\n", mag_x, mag_y, mag_z);
        printf("Acceleration X: %d, Y: %d, Z: %d\n", acc_x, acc_y, acc_z);

        sleep_ms(100); // Reduced delay to 100ms (10 times per second)
    }

    return 0;
}
