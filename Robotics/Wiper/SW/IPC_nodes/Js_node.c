#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

#define ZMQ_ENDPOINT "ipc:///tmp/servo_zmq"
#define BUTTON_CW 0   // Button to increase angle (clockwise)
#define BUTTON_CCW 1  // Button to decrease angle (counterclockwise)
#define ANGLE_STEP 10 // Degrees to change per button press

struct js_event js_event_data;
volatile int ready = 0;
volatile int done = 0; // Flag to indicate reader thread termination
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* js_reader(void* arg) {
    void *pusher = (void*)arg; // Receive pusher socket from main
    int js_fd = open("/dev/input/js0", O_RDONLY);
    if (js_fd == -1) {
        perror("Error opening joystick device");
        pthread_mutex_lock(&mtx);
        done = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mtx);
        return NULL;
    }

    int num_of_axes = 0, num_of_buttons = 0;
    ioctl(js_fd, JSIOCGAXES, &num_of_axes);
    ioctl(js_fd, JSIOCGBUTTONS, &num_of_buttons);
    printf("Joystick: %d axes, %d buttons\n", num_of_axes, num_of_buttons);

    while (1) {
        if (read(js_fd, &js_event_data, sizeof(struct js_event)) != sizeof(struct js_event)) {
            perror("Error reading joystick event");
            pthread_mutex_lock(&mtx);
            done = 1;
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&mtx);
            break;
        }
        pthread_mutex_lock(&mtx);
        ready = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mtx);
    }

    close(js_fd);
    return NULL;
}

int main() {
    // Initialize ZeroMQ context and PUSH socket
    void *context = zmq_ctx_new();
    if (!context) {
        perror("Failed to create ZeroMQ context");
        return EXIT_FAILURE;
    }
    void *pusher = zmq_socket(context, ZMQ_PUSH);
    if (!pusher) {
        perror("Failed to create ZeroMQ socket");
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }
    if (zmq_bind(pusher, ZMQ_ENDPOINT) != 0) {
        perror("Failed to bind ZeroMQ socket");
        zmq_close(pusher);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    pthread_t reader;

    if (pthread_create(&reader, NULL, js_reader, pusher) != 0) {
        perror("Failed to create reader thread");
        zmq_close(pusher);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    while (1) {
        pthread_mutex_lock(&mtx);
        while (!ready && !done) {
            pthread_cond_wait(&cond, &mtx);
        }
        if (done) {
            pthread_mutex_unlock(&mtx);
            break;
        }
        if (ready) {
            uint8_t pkg[2]; //set to pin and vlaue like in js_wiper.c
            if (js_event_data.type & JS_EVENT_BUTTON) {
                printf("Button %d %s (value: %d)\n",
                       js_event_data.number,
                       (js_event_data.value == 0) ? "released" : "pressed",
                       js_event_data.value);
            } else if (js_event_data.type & JS_EVENT_AXIS) {
                printf("Axis %d moved (value: %d)\n",
                       js_event_data.number,
                       js_event_data.value);
            } else if (js_event_data.type & JS_EVENT_INIT) {
                printf("Initial state event (type: %d, number: %d, value: %d)\n",
                       js_event_data.type, js_event_data.number, js_event_data.value);
            }
                pkg[0] = 2;
                pkg[1] = 1;// for testing, delete it
                zmq_msg_t msg;
                zmq_msg_init_size(&msg, 2*sizeof(char));
                memcpy(zmq_msg_data(&msg), pkg, 2*sizeof(char));
                if (zmq_msg_send(&msg, pusher, 0) == -1) {
                    perror("Failed to send ZeroMQ message");
                    zmq_msg_close(&msg);
                    pthread_mutex_unlock(&mtx);
                    break;
                }
                zmq_msg_close(&msg);
            ready = 0; // Reset for next event
        }
        pthread_mutex_unlock(&mtx);
    }

    pthread_join(reader, NULL);
    zmq_close(pusher);
    zmq_ctx_destroy(context);
    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cond);
    return 0;
}