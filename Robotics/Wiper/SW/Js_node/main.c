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
#include <time.h>

#define ZMQ_ENDPOINT "ipc:///tmp/servo_zmq"
#define BUTTON_CW 0   // Button to increase angle (clockwise)
#define BUTTON_CCW 1  // Button to decrease angle (counterclockwise)
#define ANGLE_STEP 10 // Degrees to change per button press

struct js_event js_event_data;
volatile int ready = 0;
volatile int done = 0; // Flag to indicate reader thread termination
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
struct timespec last_time, curr_time;

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

void* timer_action(void* arg){
    clock_gettime(CLOCK_MONOTONIC, &last_time);
    double elapsed_ms;
    while(1)
    {
        clock_gettime(CLOCK_MONOTONIC, &curr_time);
        elapsed_ms = (curr_time.tv_sec - last_time.tv_sec) * 1000.0 + (curr_time.tv_nsec - last_time.tv_nsec) / 1000000.0;
        if(elapsed_ms > 250){
            pthread_mutex_lock(&mtx);
            ready = 1;
            last_time = curr_time;
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&mtx);
        }
    }
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

    int angle = 90; // Start at midpoint (90°)
    pthread_t reader, timer;

    pthread_create(&timer,NULL, timer_action, NULL);
    pthread_detach(timer);
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

            
            if (js_event_data.type & JS_EVENT_BUTTON && js_event_data.value == 1) {
                if (js_event_data.number == BUTTON_CW) {
                    angle += ANGLE_STEP;
                    if (angle > 180) angle = 180;
                    printf("Button %d pressed: Angle increased to %d°\n", BUTTON_CW, angle);
                } else if (js_event_data.number == BUTTON_CCW) {
                    angle -= ANGLE_STEP;
                    if (angle < 0) angle = 0;
                    printf("Button %d pressed: Angle decreased to %d°\n", BUTTON_CCW, angle);
                }

                // Send angle via ZeroMQ
                zmq_msg_t msg;
                zmq_msg_init_size(&msg, sizeof(int));
                memcpy(zmq_msg_data(&msg), &angle, sizeof(int));
                if (zmq_msg_send(&msg, pusher, 0) == -1) {
                    perror("Failed to send ZeroMQ message");
                    zmq_msg_close(&msg);
                    pthread_mutex_unlock(&mtx);
                    break;
                }
                zmq_msg_close(&msg);
            }
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