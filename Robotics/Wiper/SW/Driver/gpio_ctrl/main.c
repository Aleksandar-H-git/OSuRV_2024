
#include <linux/module.h> // module_init(), module_exit()
#include <linux/fs.h> // file_operations
#include <linux/errno.h> // EFAULT
#include <linux/uaccess.h> // copy_from_user(), copy_to_user()

#include <linux/delay.h> // udelay()

MODULE_LICENSE("Dual BSD/GPL");


#include "include/gpio_ctrl.h"
#include "gpio.h"




static int gpio_stream_open(struct inode *inode, struct file *filp) {
	return 0;
}

static int gpio_stream_release(struct inode *inode, struct file *filp) {
	return 0;
}

uint8_t rd_val = 0;

static ssize_t gpio_stream_write(
	struct file* filp,
	const char *buf,
	size_t len,
	loff_t *f_pos
) {

	int i;
	uint8_t pkg[3];
	uint8_t gpio_no;
	uint8_t op;
	uint8_t wr_val;

#if 0
	printk(KERN_INFO DRV_NAME": %s() len = %d\n", __func__, len);
	for(i = 0; i < len; i++){
		printk(KERN_INFO DRV_NAME": %s() buf[%d] = %d\n", __func__, i, (int)buf[i]);
	}
#else
	(void)i;
#endif

	if(len != 3 && len != 2 && len != 1){
		return -EINVAL;
	}

	if(copy_from_user(pkg,	buf, len) != 0){
		return -EFAULT;
	}


	op = pkg[0];
	printk(KERN_INFO DRV_NAME": %s() op = %c\n", __func__, op);

	if(op == 'w' && len == 3){
		gpio_no = pkg[1];
		printk(KERN_INFO DRV_NAME": %s() gpio_num = %d\n", __func__, gpio_no);

		wr_val = pkg[2];
		printk(KERN_INFO DRV_NAME": %s() wr_val = %d\n", __func__, wr_val);

		gpio__steer_pinmux(gpio_no, GPIO__OUT);

		if(wr_val){
			gpio__set(gpio_no);
		}else{
			gpio__clear(gpio_no);
		}

	}else if(len == 2){
		gpio_no = pkg[1];
		printk(KERN_INFO DRV_NAME": %s() gpio_num = %d\n", __func__, gpio_no);
		
		gpio__steer_pinmux(gpio_no, GPIO__IN);

		if(op == 'r'){
			gpio__pull(gpio_no, GPIO__PULL_NONE);
		}else if(op == 'd'){			
			gpio__pull(gpio_no, GPIO__PULL_DOWN);
		}else if(op == 'u'){
			gpio__pull(gpio_no, GPIO__PULL_UP);			
		}else{
			return -EINVAL;
		}

		rd_val = gpio__read(gpio_no);
		printk(KERN_INFO DRV_NAME": %s() rd_val = %d\n", __func__, rd_val);
	}else{
		return -EINVAL;
	}


	return len;
}


static ssize_t gpio_stream_read(
	struct file* filp,
	char* buf,
	size_t len,
	loff_t* f_pos
) {

	if(len != 1){
		return -EINVAL;
	}

	if(copy_to_user(buf, &rd_val, len) != 0){
		return -EFAULT;
	}else{
		return len;
	}
}


static long gpio_stream_ioctl(
	struct file* filp,
	unsigned int cmd,
	unsigned long arg
) {
	switch(cmd){
		//case IOCTL_MOTOR_CLTR_SET_MODUO:
			//TODO
			//break;
		default:
			break;
	}

	return 0;
}

loff_t gpio_stream_llseek(
	struct file* filp,
	loff_t offset,
	int whence
) {
	switch(whence){
		case SEEK_SET:
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			filp->f_pos += offset;
			break;
		case SEEK_END:
			return -ENOSYS; // Function not implemented.
		default:
			return -EINVAL;
		}
	return filp->f_pos;
}

static struct file_operations gpio_stream_fops = {
	open           : gpio_stream_open,
	release        : gpio_stream_release,
	read           : gpio_stream_read,
	write          : gpio_stream_write,
	unlocked_ioctl : gpio_stream_ioctl,
	llseek         : gpio_stream_llseek
};


void gpio_ctrl_exit(void) {
	gpio__exit();

	unregister_chrdev(DEV_STREAM_MAJOR, DEV_STREAM_NAME);

	printk(KERN_INFO DRV_NAME": Module removed.\n");
}

int gpio_ctrl_init(void) {
	int r;

	printk(KERN_INFO DRV_NAME": Inserting module...\n");

	r = register_chrdev(DEV_STREAM_MAJOR, DEV_STREAM_NAME, &gpio_stream_fops);
	if(r < 0){
		printk(KERN_ERR DRV_NAME": cannot obtain major number %d!\n", DEV_STREAM_MAJOR);
		goto exit;
	}

	r = gpio__init();
	if(r){
		goto exit;
	}

exit:
	if(r){
		printk(KERN_ERR DRV_NAME": %s() failed with %d!\n", __func__, r);
		gpio_ctrl_exit();
	}else{
		printk(KERN_INFO DRV_NAME": Inserting module successful.\n");
	}
	return r;
}


module_init(gpio_ctrl_init);
module_exit(gpio_ctrl_exit);
