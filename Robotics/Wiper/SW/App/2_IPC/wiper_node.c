#include <stdio.h>
#include <zmq.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include "include/gpio_ctrl.h"
#include "gpio.h"

#define ZMQ_ENDPOINT "ipc:///tmp/servo_zmq"
#define DEV_STREAM_FN "/dev/gpio_stream"

int gpio_write(int fd, uint8_t pin, uint8_t value) {
    uint8_t pkg[3];
    pkg[0] = pin;
    pkg[1] = GPIO_CTRL__WRITE;
    pkg[2] = value;

    if (write(fd, &pkg, 3) != 3) {
        perror("Failed to write to GPIO");
        return -1;
    }
    return 0;
}

volatile int num_of_buttons = 0; // Received from joypad_node
volatile char* button_states = NULL;
pthread_mutex_t cmd_mtx = PTHREAD_MUTEX_INITIALIZER;

void* zmq_subscriber(void* arg) {
    void* context = zmq_ctx_new();
    if (!context) {
        perror("Failed to create ZeroMQ context");
        return NULL;
    }
    void* puller = zmq_socket(context, ZMQ_PULL);
    if (!puller) {
        perror("Failed to create ZeroMQ socket");
        zmq_ctx_destroy(context);
        return NULL;
    }
    if (zmq_connect(puller, ZMQ_ENDPOINT) != 0) {
        perror("Failed to connect ZeroMQ socket");
        zmq_close(puller);
        zmq_ctx_destroy(context);
        return NULL;
    }

    printf("Listening for messages on %s...\n", ZMQ_ENDPOINT);

    while (1) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int bytes = zmq_msg_recv(&msg, puller, 0); // Blocking receive
        if (bytes > 0) {
            char* buffer = (char*)malloc((bytes + 1) * sizeof(char));
            if (buffer == NULL) {
                perror("Memory allocation failed for buffer");
                zmq_msg_close(&msg);
                zmq_close(puller);
                zmq_ctx_destroy(context);
                return NULL;
            }
            memcpy(buffer, zmq_msg_data(&msg), bytes);
            buffer[bytes] = '\0';

            pthread_mutex_lock(&cmd_mtx);
            if (num_of_buttons == 0) { // First message is number of buttons
                num_of_buttons = atoi(buffer);
                if (num_of_buttons <= 0) {
                    perror("Invalid number of buttons (non-positive)");
                    free(buffer);
                    pthread_mutex_unlock(&cmd_mtx);
                    zmq_msg_close(&msg);
                    zmq_close(puller);
                    zmq_ctx_destroy(context);
                    return NULL;
                }
                // printf("Received number of buttons: %d\n", num_of_buttons);
                // printf("Allocating button_states for %d bytes at %p\n", num_of_buttons, (void*)button_states);
                button_states = (char*)malloc((num_of_buttons+1) * sizeof(char));
                if (button_states == NULL) {
                    perror("Memory allocation failed for button_states");
                    free(buffer);
                    pthread_mutex_unlock(&cmd_mtx);
                    zmq_msg_close(&msg);
                    zmq_close(puller);
                    zmq_ctx_destroy(context);
                    return NULL;
                }
                // printf("Allocated button_states at %p\n", (void*)button_states);
            } else { // Subsequent messages are button states
                if (bytes != num_of_buttons) {
                    printf("Reallocating button_states: current size %d, new size %d, old pointer %p\n",
                           num_of_buttons, bytes, (void*)button_states);
                    free(button_states);
                    button_states = NULL; // Prevent invalid pointer
                    num_of_buttons = bytes;
                    button_states = (char*)malloc((num_of_buttons+1) * sizeof(char));
                    if (button_states == NULL) {
                        perror("Memory reallocation failed for button_states");
                        free(buffer);
                        pthread_mutex_unlock(&cmd_mtx);
                        zmq_msg_close(&msg);
                        zmq_close(puller);
                        zmq_ctx_destroy(context);
                        return NULL;
                    }
                    printf("Reallocated button_states at %p\n", (void*)button_states);
                }
                memcpy(button_states, zmq_msg_data(&msg), num_of_buttons);
                button_states[num_of_buttons] = '\0'; // Ensure null termination
                printf("Received button states: %s\n", button_states);
            }
            pthread_mutex_unlock(&cmd_mtx);
            free(buffer); // Free the dynamic buffer
        }
        zmq_msg_close(&msg);
    }

    zmq_close(puller);
    zmq_ctx_destroy(context);
    return NULL;
}

int main() {
    int gpio_fd = open(DEV_STREAM_FN, O_RDWR);
    if (gpio_fd < 0) {
        perror("Failed to open /dev/gpio_stream");
        return EXIT_FAILURE;
    }

    pthread_t subscriber;
    if (pthread_create(&subscriber, NULL, zmq_subscriber, NULL) != 0) {
        perror("Failed to create subscriber thread");
        close(gpio_fd);
        return EXIT_FAILURE;
    }

    volatile int running = 1;
    while (running) {
        pthread_mutex_lock(&cmd_mtx);
        if (num_of_buttons > 0 && button_states != NULL) {
            for (int i = 0; i < num_of_buttons && i < 3; i++) { // Limit to 3 buttons
                if (button_states[i] == '1' && (i == 0 || i == 1 || i == 2)) {
                    printf("Button %d pressed\n", i);
                    switch (i) {
                        case 0: // BUTTON_CCW
                            gpio_write(gpio_fd, 3, 1); // CCW
                            gpio_write(gpio_fd, 4, 0); // CCW
                            gpio_write(gpio_fd, 2, 1); // EN = 1
                            break;
                        case 1: // BUTTON_CW
                            gpio_write(gpio_fd, 3, 0); // CW
                            gpio_write(gpio_fd, 4, 1); // CW
                            gpio_write(gpio_fd, 2, 1); // EN = 1
                            break;
                        case 2: // BUTTON_STOP
                            gpio_write(gpio_fd, 2, 0); // EN = 0
                            break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&cmd_mtx);
        usleep(10000);
    }

    pthread_join(subscriber, NULL);
    if (button_states != NULL) {
        printf("Freeing button_states at %p\n", (void*)button_states);
        free(button_states);
        button_states = NULL;
    }
    close(gpio_fd);
    pthread_mutex_destroy(&cmd_mtx);

    return 0;
}

