#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "lwip/ip4_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "FreeRTOS.h"
#include "task.h"

#define TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

#define TCP_PORT 4242
#define DEBUG_printf printf
#define BUF_SIZE 2048
#define TEST_ITERATIONS 10
#define POLL_TIME_S 5
#define WIFI_TIMEOUT_MS 30000
#define WIFI_SSID "Galaxy S22 Ultra379A"
#define WIFI_PASSWORD "euxb2562"
// #define WIFI_SSID "Koh's Family_2.4G"
// #define WIFI_PASSWORD "0001772532"

char result[4];

static char event_str[128];

// Pin Definitions
#define L_IR_SENSOR_PIN 2 // Left wheel encoder
#define R_IR_SENSOR_PIN 5 // Right wheel encoder
#define LEFT_MOTOR_PWM 0  // The GPIO pin connected to the PWM motor 1
#define RIGHT_MOTOR_PWM 1 // The GPIO pin connected to the PWM motor 2
#define MOTOR_N1_PIN 16   // The GPIO pin connected to N1 of the L298N motor controller
#define MOTOR_N2_PIN 17   // The GPIO pin connected to N2 of the L298N motor controller
#define MOTOR_N3_PIN 18   // The GPIO pin connected to N3 of the L298N motor controller
#define MOTOR_N4_PIN 19   // The GPIO pin connected to N4 of the L298N motor controller
#define BARCODE_PIN 28 // The GPIO pin connected to IR LEFT LINE SENSOR

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

// Encoder-related variables
static uint32_t NotchCount = 0;
static absolute_time_t lastNotchTime;
static bool isCountingPulse = false;
const int dutyCycle = 10;

// Wheel characteristics
static const float wheelCircumferenceMeters = 0.204; // Replace with your wheel's circumference
static const uint32_t notchesPerRevolution = 20;
static float totalDistanceMeters = 0.0;


/**
 * @brief Struct representing a TCP server.
 */
typedef struct TCP_SERVER_T_
{
    struct tcp_pcb *server_pcb;    /**< Pointer to the server's PCB (Protocol Control Block). */
    struct tcp_pcb *client_pcb;    /**< Pointer to the client's PCB. */
    bool complete;                 /**< Flag indicating if the connection is complete. */
    char buffer_sent[BUF_SIZE];    /**< Buffer for sent data. */
    uint8_t buffer_recv[BUF_SIZE]; /**< Buffer for received data. */
    int sent_len;                  /**< Length of the sent data. */
    int recv_len;                  /**< Length of the received data. */
    int recv_count;                /**< Count of the received data. */
    int run_count;                 /**< Count of the number of times the test has been run. */
} TCP_SERVER_T;

/**
 * @brief Initializes a TCP server.
 *
 * @return A pointer to the initialized TCP server, or NULL if initialization failed.
 */
static TCP_SERVER_T *tcp_server_init(void)
{
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state)
    {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    return state;
}

/**
 * @brief Closes the TCP server connection and frees associated resources.
 *
 * @param arg Pointer to the TCP server state structure.
 * @return err_t ERR_OK if successful, ERR_ABRT if the connection was aborted.
 */
static err_t tcp_server_close(void *arg)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    err_t err = ERR_OK;
    if (state->client_pcb != NULL)
    {
        tcp_arg(state->client_pcb, NULL);
        tcp_poll(state->client_pcb, NULL, 0);
        tcp_sent(state->client_pcb, NULL);
        tcp_recv(state->client_pcb, NULL);
        tcp_err(state->client_pcb, NULL);
        err = tcp_close(state->client_pcb);
        if (err != ERR_OK)
        {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(state->client_pcb);
            err = ERR_ABRT;
        }
        state->client_pcb = NULL;
    }
    if (state->client_pcb)
    {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
    return err;
}

/**
 * Callback function for TCP server to handle sent data.
 *
 * @param arg user supplied argument
 * @param tpcb TCP control block
 * @param len length of data sent
 * @return ERR_OK if successful
 */
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE)
    {

        // We should get the data back from the client
        state->recv_len = 0;
    }

    return ERR_OK;
}

/**
 * Sends data to a TCP server.
 *
 * @param arg - a void pointer to the TCP server state.
 * @param tpcb - a pointer to the TCP control block.
 * @return err_t - an error code indicating the success or failure of the operation.
 */
err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    // set the buffer to a known value, e.g: "Awaiting input: "
    memset(state->buffer_sent, 0, BUF_SIZE);
    // strcpy(state->buffer_sent, "Pico@RaspberryPi:~$ ");

    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, state->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK)
    {
        DEBUG_printf("tcp_write failed %d\n", err);
        return err;
    }
    else
    {
        DEBUG_printf("tcp_write success\n");
    }

    return ERR_OK;
}

/**
 * @brief Callback function for receiving data from a TCP client.
 *
 * @param arg Pointer to the TCP server state structure.
 * @param tpcb Pointer to the TCP control block structure.
 * @param p Pointer to the received data buffer.
 * @param err Error code for the receive operation.
 * @return Error code for the function.
 */
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;

    if (!p)
    {
        printf("pbuf is null\n");
        return ERR_BUF;
    }

    cyw43_arch_lwip_check();
    // check if we received a "help" message
    if (strcmp((char *)p->payload, "help") == 0)
    {
        printf("help message received\n");
        strcpy(state->buffer_sent, "Available commands:\n");
        strcat(state->buffer_sent, "help - display this message\n");

        // send the response
        err_t err = tcp_write(tpcb, state->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK)
        {
            DEBUG_printf("tcp_write failed %d\n", err);
            return err;
        }
        else
        {
            DEBUG_printf("tcp_write success\n");
        }
    }

    printf("tcp_server_recv %u\n", p->tot_len);
    printf("payload value: %s\n", (char *)p->payload);

    tcp_server_send_data(arg, state->client_pcb);

    pbuf_free(p);

    return ERR_OK;
}

static void tcp_server_err(void *arg, err_t err)
{
    if (err != ERR_ABRT)
    {
        DEBUG_printf("tcp_client_err_fn %d\n", err);
    }
}

/**
 * Callback function for accepting a new TCP client connection.
 *
 * @param arg Pointer to the TCP server state structure.
 * @param client_pcb Pointer to the new client's TCP control block.
 * @param err Error code for the connection attempt.
 * @return ERR_VAL if there was an error, otherwise the result of tcp_server_send_data().
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    if (err != ERR_OK || client_pcb == NULL)
    {
        DEBUG_printf("Failure in accept\n");
        return ERR_VAL;
    }
    DEBUG_printf("Client connected\n");

    state->client_pcb = client_pcb;
    tcp_arg(client_pcb, state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_err(client_pcb, tcp_server_err);

    return tcp_server_send_data(arg, state->client_pcb);
}

/**
 * Opens a TCP server on the specified port and listens for incoming connections.
 *
 * @param arg A pointer to a TCP_SERVER_T struct containing the server state.
 * @return true if the server was successfully started, false otherwise.
 */
static bool tcp_server_open(void *arg)
{
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    DEBUG_printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb)
    {
        DEBUG_printf("failed to create pcb\n");
        return false;
    }

    err_t err = tcp_bind(pcb, NULL, TCP_PORT);
    if (err)
    {
        DEBUG_printf("failed to bind to port %u\n", TCP_PORT);
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb)
    {
        DEBUG_printf("failed to listen\n");
        if (pcb)
        {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    return true;
}

// sending message to tcp server
void send_tcp_message(TCP_SERVER_T *state, const char *message)
{
    memset(state->buffer_sent, 0, BUF_SIZE);
    strcpy(state->buffer_sent, message);
    // send the response
    err_t err = tcp_write(state->client_pcb, state->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK)
    {
        DEBUG_printf("tcp_write failed %d\n", err);
    }
    else
    {
        DEBUG_printf("tcp_write success\n");
        // Flush the data
        tcp_output(state->client_pcb);
    }
}

// struct for dictionary
struct KeyValuePair
{
    char key;
    const char *value;
    const char *reverse;
};

// Searching from array to find matches
void findAndPrintMatches(const char *input, struct KeyValuePair dictionary[], int dictionarySize, char result[])
{
    int length = strlen(input);
    int resultIndex = 0;

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
                result[resultIndex++] = dictionary[j].key;
            }
        }
        result[resultIndex++] = '\n'; // new Line
        result[resultIndex] = '\0';  // Null-terminate the result
    }
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
    pwm_set_clkdiv(sliceNum1, 100);
    pwm_set_wrap(sliceNum1, 12500);
    pwm_set_chan_level(sliceNum1, PWM_CHAN_A, 4000);
    pwm_set_chan_level(sliceNum1, PWM_CHAN_B, 4000);

    pwm_set_clkdiv(sliceNum2, 100);
    pwm_set_wrap(sliceNum2, 12500);
    pwm_set_chan_level(sliceNum2, PWM_CHAN_A, 6000);
    pwm_set_chan_level(sliceNum2, PWM_CHAN_B, 6000);

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

// interrupt call back for the motor and barcode ir sensor
void gpioCallback(uint gpio, uint32_t events)
{
    gpio_event_string(event_str, events);

    // interrupt when trigger motor pin
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

     // interrupt when trigger barcode ir sensor pin
    if (gpio == BARCODE_PIN)
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
}

// checking for barcode is reverse or not.
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

// calculate average for moving 
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

// reset everything for the barcode when it touch white space more than 1 second.
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
    // Empty the array by setting all elements to '0'
    memset(result, '\0', sizeof(result));
}

/**
 * @brief Task that runs a TCP server and connects to Wi-Fi.
 *
 * This function initializes the CYW43 Wi-Fi module, connects to a Wi-Fi network,
 * initializes a TCP server, and enters an infinite loop to keep the server running.
 *
 * @param params Unused parameter required by FreeRTOS.
 */
void server_task(__unused void *params)
{
    int timeout_code = 0;

    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    timeout_code = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, WIFI_TIMEOUT_MS);
    if (timeout_code != 0)
    {
        printf("failed to connect. %d\n", timeout_code);
        return;
    }
    else
    {
        printf("Connected.\n");
    }

    TCP_SERVER_T *state = tcp_server_init();
    if (!state)
    {
        printf("failed to allocate state\n");
        return;
    }
    if (!tcp_server_open(state))
    {
        printf("failed to open server\n");
        return;
    }

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

    while (true)
    {        //vTaskDelay(1000);
        // checking tcp is conntected
        if (state->client_pcb && state->client_pcb->state == ESTABLISHED)
        {
            // let the motor move forward
            moveForward();

            // Barcode IR Line Sensor Edge Rise is detected
            if (isRise)
            {
                // when white space is found between the barcode
                if (isGap)
                {
                    // end the time when detect black.
                    endTimeWhite = to_ms_since_boot(get_absolute_time());
                    // getting the gap width value
                    whiteWidth = endTimeWhite - startTimeWhite;
                    // when detect white space more than 1 second, reset barcode related to default
                    if (whiteWidth > 1000)
                    {
                        printf("Too long, reset all to default\n");
                        resetEverything();
                    } else {
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
            // Barcode IR Line Sensor Edge Fail is detected
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

                // when count reach 9 can check the barcode is reverse or not.
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
                        //end_tcp_message(state, binaryString);
                        // findAndPrintMatches will split the array into size 16 and check the value match the dictionary value
                        findAndPrintMatches(binaryString, dictionary, sizeof(dictionary) / sizeof(dictionary[0]), result);
                        // display new line after the matches
                        printf("\n");
                        send_tcp_message(state, result);
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
                                }
                                else if (i % 2 == 1)
                                {
                                    // hard coded for loop 3 time to add 3 '0' to new char array
                                    for (int j = 0; j < 3; j++)
                                    {
                                        binaryString[j_counter] = '0';
                                        j_counter++;
                                    }
                                }
                            }
                            else
                            {
                                // checking is black or white
                                // %2 == 0 mean is black
                                if (i % 2 == 0)
                                {
                                    // add '1' to new char array
                                    binaryString[j_counter] = '1';
                                    j_counter++;
                                }
                                else if (i % 2 == 1)
                                {
                                    // add '0' to new char array
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
                        // send_tcp_message(state, binaryString);
                        // findAndPrintMatches will split the array into size 16 and check the value match the dictionary value
                        findAndPrintMatches(binaryString, dictionary, sizeof(dictionary) / sizeof(dictionary[0]), result);
                        // display new line after the matches
                        // printf("\n");
                        send_tcp_message(state, result);
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
}

int main(void)
{
    stdio_init_all();

    sleep_ms(4000);

    gpio_init(BARCODE_PIN);
    gpio_disable_pulls(BARCODE_PIN);

    gpio_init(MOTOR_N1_PIN);
    gpio_init(MOTOR_N2_PIN);
    gpio_init(MOTOR_N3_PIN);
    gpio_init(MOTOR_N4_PIN);
    gpio_set_dir(MOTOR_N1_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N2_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N3_PIN, GPIO_OUT);
    gpio_set_dir(MOTOR_N4_PIN, GPIO_OUT);

    initializePwmForMotor(dutyCycle);

    gpio_set_irq_enabled_with_callback(L_IR_SENSOR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);
    gpio_set_irq_enabled_with_callback(BARCODE_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpioCallback);

    TaskHandle_t server;
    xTaskCreate(server_task, "ServerTask", configMINIMAL_STACK_SIZE, NULL, 4, &server);

    vTaskStartScheduler();

    return 0;
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