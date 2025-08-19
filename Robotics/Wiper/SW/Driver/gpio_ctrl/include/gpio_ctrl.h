
#ifndef GPIO_CLTR_H
#define GPIO_CLTR_H

// For uint8_t.
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif


#define DRV_NAME "gpio_ctrl"
#define DEV_STREAM_MAJOR 261
#define DEV_STREAM_NAME "gpio_stream"
#define DEV_STREAM_FN "/dev/"DEV_STREAM_NAME


typedef enum {
	GPIO_CTRL__READ = 'r',
	GPIO_CTRL__WRITE = 'w',
} gpio_ctrl__gpio_cmd_t;

typedef struct {
	uint8_t gpio_no;
	uint8_t op;
	uint8_t wr_val;
} gpio_ctrl__stream_pkg_t;



#endif // GPIO_CLTR_H
