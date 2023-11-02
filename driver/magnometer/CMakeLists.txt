add_executable(projectMagnetometer
projectMagnetometer.c
        )

# pull in common dependencies and additional i2c hardware support
target_link_libraries(projectMagnetometer pico_stdlib hardware_i2c)

# create map/bin/hex file etc.
pico_add_extra_outputs(projectMagnetometer)

# add url via pico_set_program_url
example_auto_set_url(projectMagnetometer)

pico_enable_stdio_usb(projectMagnetometer 1)
pico_enable_stdio_uart(projectMagnetometer 0)