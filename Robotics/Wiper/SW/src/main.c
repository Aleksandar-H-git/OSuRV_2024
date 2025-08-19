#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

struct js_event js_event_data;
volatile int ready = 0;
volatile int done = 0; // Flag to indicate reader thread termination
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* js_reader(void* arg) {
    int js_fd;
    int num_of_axes = 0;
    int num_of_buttons = 0;

    // Open the joystick device file in read-only mode
    js_fd = open("/dev/input/js0", O_RDONLY);
    if (js_fd == -1) {
        perror("Error opening joystick device");
        pthread_mutex_lock(&mtx);
        done = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mtx);
        return NULL;
    }

    ioctl(js_fd, JSIOCGAXES, &num_of_axes);
    ioctl(js_fd, JSIOCGBUTTONS, &num_of_buttons);

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
    pthread_t reader;

	pthread_create(&reader, NULL, js_reader, NULL);

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
            ready = 0;
        }
        pthread_mutex_unlock(&mtx);
    }

    pthread_join(reader, NULL);

    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cond);

    return 0;
}