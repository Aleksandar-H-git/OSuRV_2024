// #include <stdio.h>
// #include <zmq.h>
// #include <string.h>
// #include <unistd.h>
// #define ZMQ_ENDPOINT "ipc:///tmp/servo_zmq"

// int main() {
//     void *context = zmq_ctx_new();
//     void *puller = zmq_socket(context, ZMQ_PULL);
//     zmq_connect(puller, ZMQ_ENDPOINT);

//     printf("Listening for messages on %s...\n", ZMQ_ENDPOINT);

//     while (1) {
//         zmq_msg_t msg;
//         zmq_msg_init(&msg);
//         int bytes = zmq_msg_recv(&msg, puller, ZMQ_DONTWAIT);
//         if (bytes == 2*sizeof(uint8_t)) {
//             uint8_t pkg[2];
//             memcpy(pkg, zmq_msg_data(&msg), 2*sizeof(char));
//             printf("pkg[0]: %d\n", pkg[0]);
//             printf("pkg[1]: %d\n", pkg[1]);
//         }
//         zmq_msg_close(&msg);
//         usleep(10000);
//     }

//     zmq_close(puller);
//     zmq_ctx_destroy(context);
//     return 0;
// }

#include <stdio.h>
#include <zmq.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "include/gpio_ctrl.h"
 
#include "gpio.h"

#define ZMQ_ENDPOINT "ipc:///tmp/servo_zmq"
#define DEV_STREAM_FN "/dev/gpio_stream" // Adjust if needed

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

int main() {
    // Initialize ZeroMQ
    void *context = zmq_ctx_new();
    if (!context) {
        perror("Failed to create ZeroMQ context");
        return EXIT_FAILURE;
    }
    void *puller = zmq_socket(context, ZMQ_PULL);
    if (!puller) {
        perror("Failed to create ZeroMQ socket");
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }
    if (zmq_connect(puller, ZMQ_ENDPOINT) != 0) {
        perror("Failed to connect ZeroMQ socket");
        zmq_close(puller);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    // Open GPIO device
    int gpio_fd = open(DEV_STREAM_FN, O_RDWR);
    if (gpio_fd < 0) {
        perror("Failed to open /dev/gpio_stream");
        zmq_close(puller);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    printf("Listening for messages on %s...\n", ZMQ_ENDPOINT);

    while (1) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int bytes = zmq_msg_recv(&msg, puller, 0); // Blocking receive
        if (bytes > 0 && bytes < 10) {
            char buffer[10];
            memcpy(buffer, zmq_msg_data(&msg), bytes);
            buffer[bytes] = '\0';
            int button_number = atoi(buffer);
            printf("Received button number: %d\n", button_number);

            uint8_t pin, value;
            if (button_number == 1) { // CCW (from your second code)
                pin = 4;
                value = 0;
                gpio_write(gpio_fd, pin, value);

                pin = 3;
                value = 1;
                gpio_write(gpio_fd, pin, value);

                pin = 2;
                value = 1;
                gpio_write(gpio_fd, pin, value);
            } else if (button_number == 2) { // CW
                pin = 4;
                value = 1;
                gpio_write(gpio_fd, pin, value);

                pin = 3;
                value = 0;
                gpio_write(gpio_fd, pin, value);

                pin = 2;
                value = 1;
                gpio_write(gpio_fd, pin, value);
            } else if (button_number == 3) { // STOP
                pin = 2;
                value = 0;
                gpio_write(gpio_fd, pin, value);
            } else {
                printf("Unknown button: %d\n", button_number);
            }
        } else if (bytes >= 10) {
            printf("Message too large: %d bytes\n", bytes);
        }
        zmq_msg_close(&msg);

    }

    close(gpio_fd);
    zmq_close(puller);
    zmq_ctx_destroy(context);
    return 0;
}