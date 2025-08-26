
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

#include "include/gpio_ctrl.h"
#include "gpio.h"

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
#define SERVO_PIN 18        // GPIO pin (2-26); adjust as needed
#define PWM_PERIOD_US 20000 // 20ms (50Hz for servo)
#define MIN_PULSE_US 1000   // 1ms for 0°
#define MAX_PULSE_US 2000   // 2ms for 180°
#define ANGLE_STEP 10       // Degrees to change per button press
#define BUTTON_CW 0         // Button index for clockwise (increase angle)
#define BUTTON_CCW 1        // Button index for counterclockwise (decrease angle)

struct js_event js_event_data;
volatile int ready = 0;
volatile int done = 0; 
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

    int angle = 90;
    // Open GPIO device
    int gpio_fd = open(DEV_STREAM_FN, O_RDWR);
    
    if (gpio_fd < 0) {
        perror("Failed to open /dev/gpio_stream");
        return EXIT_FAILURE;
    }
    printf("Controlling servo on GPIO %d... Press Ctrl+C to exit.\n", SERVO_PIN);
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
            uint8_t pin, value;
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
                if (js_event_data.number == 1) { // CCW BUTTON
                    pin=4;
                    value=0;
                    gpio_write(gpio_fd, pin, value); 

                    pin=3;
                    value=1;
                    gpio_write(gpio_fd, pin, value);

                    pin=2;
                    value=1;
                    gpio_write(gpio_fd, pin, value);  
                } else if (js_event_data.number == 2) { // CW BUTTON
                    pin=4;
                    value=1;
                    gpio_write(gpio_fd, pin, value); 

                    pin=3;
                    value=0;
                    gpio_write(gpio_fd, pin, value);

                    pin=2;
                    value=1;
                    gpio_write(gpio_fd, pin, value);
                }else if (js_event_data.number == 3) { //STOP BUTTON - X
                    pin=2;
                    value=0;
                    gpio_write(gpio_fd, pin, value);
                }
            }

        }
        pthread_mutex_unlock(&mtx);


    }
    gpio_write(gpio_fd, SERVO_PIN, 0); // Set pin low //WRONG MODIFY
    close(gpio_fd);
    
    pthread_join(reader, NULL);

    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cond);

    return 0;
}