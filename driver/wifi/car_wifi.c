#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"

#include "lwip/ip4_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "FreeRTOS.h"
#include "task.h"

#define TEST_TASK_PRIORITY				( tskIDLE_PRIORITY + 1UL )

#define TCP_PORT 4242
#define DEBUG_printf printf
#define BUF_SIZE 2048
#define TEST_ITERATIONS 10
#define POLL_TIME_S 5
#define WIFI_TIMEOUT_MS 30000

/**
 * @brief Struct representing a TCP server.
 */
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb* server_pcb; /**< Pointer to the server's PCB (Protocol Control Block). */
    struct tcp_pcb* client_pcb; /**< Pointer to the client's PCB. */
    bool complete; /**< Flag indicating if the connection is complete. */
    char buffer_sent[BUF_SIZE]; /**< Buffer for sent data. */
    uint8_t buffer_recv[BUF_SIZE]; /**< Buffer for received data. */
    int sent_len; /**< Length of the sent data. */
    int recv_len; /**< Length of the received data. */
    int recv_count; /**< Count of the received data. */
    int run_count; /**< Count of the number of times the test has been run. */
} TCP_SERVER_T;

/**
 * @brief Initializes a TCP server.
 *
 * @return A pointer to the initialized TCP server, or NULL if initialization failed.
 */
static TCP_SERVER_T* tcp_server_init(void) {
    TCP_SERVER_T* state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
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
static err_t tcp_server_close(void* arg) {
    TCP_SERVER_T* state = (TCP_SERVER_T*)arg;
    err_t err = ERR_OK;
    if (state->client_pcb != NULL) {
        tcp_arg(state->client_pcb, NULL);
        tcp_poll(state->client_pcb, NULL, 0);
        tcp_sent(state->client_pcb, NULL);
        tcp_recv(state->client_pcb, NULL);
        tcp_err(state->client_pcb, NULL);
        err = tcp_close(state->client_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(state->client_pcb);
            err = ERR_ABRT;
        }
        state->client_pcb = NULL;
    }
    if (state->client_pcb) {
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
static err_t tcp_server_sent(void* arg, struct tcp_pcb* tpcb, u16_t len) {
    TCP_SERVER_T* state = (TCP_SERVER_T*)arg;
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE) {

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
err_t tcp_server_send_data(void* arg, struct tcp_pcb* tpcb) {
    TCP_SERVER_T* state = (TCP_SERVER_T*)arg;

    // set the buffer to a known value, e.g: "Awaiting input: "
    memset(state->buffer_sent, 0, BUF_SIZE);
    strcpy(state->buffer_sent, "Pico@RaspberryPi:~$ ");

    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, state->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        DEBUG_printf("tcp_write failed %d\n", err);
        return err;
    }
    else {
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
err_t tcp_server_recv(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    TCP_SERVER_T* state = (TCP_SERVER_T*)arg;

    if (!p) {
        printf("pbuf is null\n");
        return ERR_BUF;
    }

    cyw43_arch_lwip_check();
    // check if we received a "help" message
    if (strcmp((char*)p->payload, "help") == 0) {
        printf("help message received\n");
        strcpy(state->buffer_sent, "Available commands:\n");
        strcat(state->buffer_sent, "help - display this message\n");

        // send the response
        err_t err = tcp_write(tpcb, state->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            DEBUG_printf("tcp_write failed %d\n", err);
            return err;
        }
        else {
            DEBUG_printf("tcp_write success\n");
        }
    }

    printf("tcp_server_recv %u\n", p->tot_len);
    printf("payload value: %s\n", (char*)p->payload);

    tcp_server_send_data(arg, state->client_pcb);


    pbuf_free(p);

    return ERR_OK;
}

static void tcp_server_err(void* arg, err_t err) {
    if (err != ERR_ABRT) {
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
static err_t tcp_server_accept(void* arg, struct tcp_pcb* client_pcb, err_t err) {
    TCP_SERVER_T* state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
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
static bool tcp_server_open(void* arg) {
    TCP_SERVER_T* state = (TCP_SERVER_T*)arg;
    DEBUG_printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);

    struct tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        DEBUG_printf("failed to create pcb\n");
        return false;
    }

    err_t err = tcp_bind(pcb, NULL, TCP_PORT);
    if (err) {
        DEBUG_printf("failed to bind to port %u\n", TCP_PORT);
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb) {
        DEBUG_printf("failed to listen\n");
        if (pcb) {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    return true;
}

/**
 * @brief Task that runs a TCP server and connects to Wi-Fi.
 *
 * This function initializes the CYW43 Wi-Fi module, connects to a Wi-Fi network,
 * initializes a TCP server, and enters an infinite loop to keep the server running.
 *
 * @param params Unused parameter required by FreeRTOS.
 */
void server_task(__unused void* params) {
    int timeout_code = 0;

    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    timeout_code = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, WIFI_TIMEOUT_MS);
    if (timeout_code != 0) {
        printf("failed to connect. %d\n", timeout_code);
        return;
    }
    else {
        printf("Connected.\n");
    }

    TCP_SERVER_T* state = tcp_server_init();
    if (!state) {
        printf("failed to allocate state\n");
        return;
    }
    if (!tcp_server_open(state)) {
        printf("failed to open server\n");
        return;
    }

    while (true) {
        vTaskDelay(1000);
    }
}

int main(void)
{
    stdio_init_all();

    sleep_ms(4000);

    TaskHandle_t server;
    xTaskCreate(server_task, "ServerTask", configMINIMAL_STACK_SIZE, NULL, 4, &server);

    vTaskStartScheduler();

    return 0;
}
