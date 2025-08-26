#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <semaphore.h>
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
// #define DEV_STREAM_FN "/dev/gpio_stream"


#define BUTTON_CW 0         // Button index for clockwise (increase angle)
#define BUTTON_CCW 1        // Button index for counterclockwise (decrease angle)

struct js_event js_event_data;

volatile uint8_t* volatile buttons;
volatile int num_of_buttons = 0;
pthread_mutex_t button_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t button_printf_mtx = PTHREAD_MUTEX_INITIALIZER;
sem_t buttons_intitialized;

void* js_reader(void* arg) {
	void* pusher = arg;
	int js_fd;
	int num_of_axes = 0;
	int num_of_buttons = 0;

	// Open the joystick device file in read-only mode
	js_fd = open("/dev/input/js0", O_RDONLY);
	if (js_fd == -1) {
		perror("Error opening joystick device");
		return NULL;
	}

	ioctl(js_fd, JSIOCGAXES, &num_of_axes);
	ioctl(js_fd, JSIOCGBUTTONS, &num_of_buttons);

	//TODO Allocated buttons array
	buttons = (volatile uint8_t*)malloc(num_of_buttons * sizeof(uint8_t));
    if (buttons == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }
	// Initialize buttons to 0
    for (int i = 0; i < num_of_buttons; i++) {
        buttons[i] = 0;
    }
	printf("Joystick initialized with %d buttons\n", num_of_buttons);

	// Dynamically allocate buffer for number of buttons
    int num_buf_size = snprintf(NULL, 0, "%d", num_of_buttons) + 1; // +1 for null terminator
    char* num_buf = (char*)malloc(num_buf_size * sizeof(char));
    if (num_buf == NULL) {
        return NULL;
    }
    snprintf(num_buf, num_buf_size, "%d", num_of_buttons);
    if (zmq_send(pusher, num_buf, strlen(num_buf), 0) == -1) {
        perror("Failed to send number of buttons");
    }
    free(num_buf); // Free the dynamic buffer after sending

	sem_post(&buttons_intitialized);

	while (1) {
		if (read(js_fd, &js_event_data, sizeof(struct js_event)) != sizeof(struct js_event)) {
			perror("Error reading joystick event");
			break;
		}

		if (js_event_data.type & JS_EVENT_BUTTON) {
            pthread_mutex_lock(&button_mtx);
			if (buttons[js_event_data.number] != js_event_data.value) {
                buttons[js_event_data.number] = js_event_data.value;
                // Create string of button states
                char state_buf[2 * num_of_buttons + 1]; // "0" or "1" per button + null terminator
                for (int i = 0; i < num_of_buttons; i++) {
                    state_buf[i] = buttons[i] ? '1' : '0';
                }
                state_buf[num_of_buttons] = '\0';
                printf("Sending button states: %s\n", state_buf);
                if (zmq_send(pusher, state_buf, num_of_buttons, 0) == -1) {
                    perror("Failed to send button states");
                }
            }
            pthread_mutex_unlock(&button_mtx);
         } 
		// else if (js_event_data.type & JS_EVENT_AXIS) {
		//  	printf("Axis %d moved (value: %d)\n",
		//  		js_event_data.number,
		//  		js_event_data.value
		// 	);
	    // }
	}

	close(js_fd);
	return NULL;
}

int main() {
    // Initialize ZeroMQ context and PUSH socket
    void* context = zmq_ctx_new(); 
    if (!context) {
        perror("Failed to create ZeroMQ context");
        return EXIT_FAILURE;
    }
    void* pusher = zmq_socket(context, ZMQ_PUSH);
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

    sem_init(&buttons_intitialized, 0, 0);

    pthread_t reader;
    if (pthread_create(&reader, NULL, js_reader, pusher) != 0) {
        perror("Failed to create reader thread");
        zmq_close(pusher);
        zmq_ctx_destroy(context);
        sem_destroy(&buttons_intitialized);
        return EXIT_FAILURE;
    }

    sem_wait(&buttons_intitialized);

    uint8_t* prev_buttons = (uint8_t*)malloc(num_of_buttons * sizeof(uint8_t));
    if (prev_buttons == NULL) {
        perror("Memory allocation failed for prev_buttons");
        zmq_close(pusher);
        zmq_ctx_destroy(context);
        sem_destroy(&buttons_intitialized);
        return EXIT_FAILURE;
    }

    // Initialize prev_buttons to 0
    for (int i = 0; i < num_of_buttons; i++) {
        prev_buttons[i] = 0;
    }

    volatile int running = 1;
    while (running) {
        pthread_mutex_lock(&button_mtx);
        for (int i = 0; i < num_of_buttons; i++) {
            if (buttons && buttons[i] != prev_buttons[i]) {
                prev_buttons[i] = buttons[i];
            }
        }
        pthread_mutex_unlock(&button_mtx);
        usleep(10000);
    }

    // Cleanup
    pthread_join(reader, NULL);
    if (buttons != NULL) {
        free((void*)buttons);
        buttons = NULL;
    }
    if (prev_buttons != NULL) {
        free(prev_buttons);
        prev_buttons = NULL;
    }
    zmq_close(pusher);
    zmq_ctx_destroy(context);
    sem_destroy(&buttons_intitialized);
    pthread_mutex_destroy(&button_mtx);

    return 0;
}