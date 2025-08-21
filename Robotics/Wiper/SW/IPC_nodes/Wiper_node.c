#include <stdio.h>
#include <zmq.h>
#include <string.h>
#include <unistd.h>
#define ZMQ_ENDPOINT "ipc:///tmp/servo_zmq"

int main() {
    void *context = zmq_ctx_new();
    void *puller = zmq_socket(context, ZMQ_PULL);
    zmq_connect(puller, ZMQ_ENDPOINT);

    printf("Listening for messages on %s...\n", ZMQ_ENDPOINT);

    while (1) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        int bytes = zmq_msg_recv(&msg, puller, ZMQ_DONTWAIT);
        if (bytes == sizeof(int)) {
            int angle;
            memcpy(&angle, zmq_msg_data(&msg), sizeof(int));
            printf("Received angle: %d\n", angle);
        }
        zmq_msg_close(&msg);
        usleep(10000);
    }

    zmq_close(puller);
    zmq_ctx_destroy(context);
    return 0;
}