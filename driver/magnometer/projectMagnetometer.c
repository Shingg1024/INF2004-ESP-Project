#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define HMC5883L_ADDR 0x1E
#define CONFIG_REG_A 0x00
#define MODE_REG 0x02
#define DATA_OUTPUT_X_MSB 0x03

void hmc5883l_init(i2c_inst_t *i2c) {
    uint8_t init_data[2] = {CONFIG_REG_A, 0x70}; // Set to continuous measurement mode
    i2c_write_blocking(i2c, HMC5883L_ADDR, init_data, 2, false);

    uint8_t mode_data[2] = {MODE_REG, 0x00}; // Set to continuous measurement mode
    i2c_write_blocking(i2c, HMC5883L_ADDR, mode_data, 2, false);
}

void read_magnetic_field(i2c_inst_t *i2c, int16_t *mag_data) {
    uint8_t reg = DATA_OUTPUT_X_MSB;
    uint8_t data[6];

    i2c_write_blocking(i2c, HMC5883L_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c, HMC5883L_ADDR, data, 6, false);

    for (int i = 0; i < 3; i++) {
        mag_data[i] = (data[i * 2] << 8) | data[i * 2 + 1];
    }
}

float calculate_heading(int16_t x, int16_t y) {
    float heading = atan2(y, x) * (180.0 / M_PI);
    if (heading < 0) {
        heading += 360.0;
    }
    return heading;
}

int main() {
    stdio_init_all();

    i2c_inst_t *i2c = i2c0;
    i2c_init(i2c, 100000);
    gpio_set_function(0, GPIO_FUNC_I2C); // Assign I2C pins
    gpio_set_function(1, GPIO_FUNC_I2C);

    hmc5883l_init(i2c);

    int16_t magnetic_data[3];

    while (1) {
        read_magnetic_field(i2c, magnetic_data);

        int16_t mag_x = magnetic_data[0];
        int16_t mag_y = magnetic_data[1];
        int16_t mag_z = magnetic_data[2];

        float heading = calculate_heading(mag_x, mag_y);

        printf("Heading: %.2f degrees\n", heading);
        printf("Magnetic Field X: %d, Y: %d, Z: %d\n", mag_x, mag_y, mag_z);

        sleep_ms(100); // Reduced delay to 100ms (10 times per second)
    }

    return 0;
}
