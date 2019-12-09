#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	// printk()
#include <linux/slab.h>		// kmalloc()
#include <linux/fs.h>		// everything
#include <linux/errno.h>	// error codes
#include <linux/types.h>	// size_t
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	// O_ACCMODE
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/switch_to.h>	// cli(), *_flags
#include <asm/uaccess.h>	// copy_*_user


// if needed, use copy_*_user instead of asm/uaccess
#ifndef __ASM_ASM_UACCESS_H
    #include <linux/uaccess.h>
#endif

#include "mastermind_ioctl.h"

#define MASTERMIND_MAJOR 0
#define MASTERMIND_GUESS_SIZE 16
#define MASTERMIND_GUESS_LIMIT 10

int mastermind_init_module(void);
void mastermind_cleanup_module(void);

static char *mmind_number = "0000";

int mastermind_major = MASTERMIND_MAJOR;
int mastermind_minor = 0;
int mastermind_guess_limit = MASTERMIND_GUESS_LIMIT;

module_param(mmind_number, charp, 0000);
MODULE_PARM_DESC(mmind_number, "s");		// making the variable command line argument rather than just a global variable

module_param(mastermind_guess_limit, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mastermind_guess_limit, "An integer");

module_param(mastermind_major, int, S_IRUGO);
module_param(mastermind_minor, int, S_IRUGO);

MODULE_AUTHOR("ITU");
MODULE_LICENSE("Dual BSD/GPL");

struct mastermind_dev {
    char **guess;
    int guess_size;
    int guess_limit;
	char *mmind_number;
    int guess_count;
    struct semaphore sem;
    struct cdev cdev;
};

struct mastermind_dev *mastermind_devices;

char* toCharArray(unsigned long arg) {
    int i;
    char* result = kmalloc(5 * sizeof(char *), GFP_KERNEL);

    for(i = 3; i >= 0; i--) {
        result[i] = (arg % 10) + '0';
        arg = arg / 10;
    }
    result[4] = '\0';
    return result;
}

char* guess_handler(const char* guess, const char* mmind_number, int guess_count) {
	char* result = kmalloc(16 * sizeof(char *), GFP_KERNEL);
	int count_in_the_right_place = 0;
	int count_in_the_wrong_place = 0;

	int numbers[10] = { 0 };

    int i;
	for (i = 0; i < 4; i++)
		numbers[mmind_number[i] - '0'] = 1;

	for (i = 0; i < 4; i++) {
		if (mmind_number[i] == guess[i]) {
			count_in_the_right_place++;
		}
		else if (numbers[guess[i] - '0'] == 1) {
			count_in_the_wrong_place++;
		}
	}


	for (i = 0; i < 4; i++)
			result[i] = guess[i];		// writing the guess to result

	if (guess_count == 10) {
		result[13] = '1';
		result[14] = '0';
	}
	else {
		result[13] = '0';
		result[14] = guess_count + '0';
	}	
	result[4] = result[7] = result[10] = ' ';
	result[5] = count_in_the_right_place + '0';
	result[6] = '+';
	result[8] = count_in_the_wrong_place + '0';
	result[9] = '-';
	result[11] = result[12] = '0';
	result[15] = '\n';

	return result;
}

int clear_guesses(struct mastermind_dev *dev) {     // delete all guesses and set count to 0
    int i;

    if (dev->guess) {
        for (i = 0; i < dev->guess_limit; i++) {
            if (dev->guess[i])
                kfree(dev->guess[i]);
        }
    }
    dev->guess_count = 0;

    return 0;
}

int mastermind_trim(struct mastermind_dev *dev) {   // delete everything from memory
    int i;

    if (dev->guess) {
        for (i = 0; i < dev->guess_limit; i++) {
            if (dev->guess[i])
                kfree(dev->guess[i]);
        }
        kfree(dev->guess);
    }
    dev->guess = NULL;
    dev->guess_size = MASTERMIND_GUESS_SIZE;
    dev->guess_limit = mastermind_guess_limit;
    dev->guess_count = 0;
    return 0;
}

int mastermind_open(struct inode *xnode, struct file *xfile) {

    struct mastermind_dev *dev;

    dev = container_of(xnode->i_cdev, struct mastermind_dev, cdev);
    xfile->private_data = dev;

    /* trim the device if open was write-only */
    if ((xfile->f_flags & O_APPEND) == 0 && (xfile->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible(&dev->sem)){
            printk(KERN_INFO "mastermind: ERR %s\n", __FUNCTION__);
            return -ERESTARTSYS;
        }
        //mastermind_trim(dev);
        up(&dev->sem);
    }
    printk(KERN_INFO "mastermind: Inside %s\n", __FUNCTION__);
    return 0;
}

ssize_t mastermind_read(struct file *xfile, char __user *buf, size_t length, loff_t *offset) {
    struct mastermind_dev *dev = xfile->private_data;
    int guess_size = dev->guess_size;
    int guess_count = dev->guess_count;
    int i, j, buf_pos = 0;
    ssize_t return_value = 0;
    char *local_buffer;

    local_buffer = kmalloc(guess_count * guess_size * sizeof(char *), GFP_KERNEL);

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    if (*offset >= dev->guess_count)
        goto out;

    if (dev->guess == NULL || ! dev->guess[0])
        goto out;

    for(i = 0; i < guess_count; i++) {
        for(j = 0; j < guess_size; j++) {
            local_buffer[buf_pos] = *(dev->guess[i] + j);
            buf_pos++;
        }
    }

    if (copy_to_user(buf, local_buffer, buf_pos)) {
        return_value = -EFAULT;
        goto out;
    }
    *offset += buf_pos;
    return_value = buf_pos;

  out:
    up(&dev->sem);
    return return_value;
}

ssize_t mastermind_write(struct file *xfile, const char __user *buf, size_t length, loff_t *offset) {

	struct mastermind_dev *dev = xfile->private_data;
    int guess_size = dev->guess_size;	// Which is 16
	int guess_limit = dev->guess_limit;	// Which is 10
	int guess_count = dev->guess_count;	// Which is variable btw 0 and guess limit
	char *mmind_number = dev->mmind_number;
    //int s_pos;
    ssize_t return_value = -ENOMEM;

	// GUESS TO HANDLED GUESS
	char *result_buffer;
    char *guess_buffer;

    if (down_interruptible(&dev->sem)){
        printk(KERN_INFO "mastermind: ERR %s\n", __FUNCTION__);
        return -ERESTARTSYS;
    }

    if (guess_count >= guess_limit) {   // checking if there are already enough guesses
        printk(KERN_INFO "Error. The number of guesses reached the limit. You cannot enter a guess anymore.");  // error message
        return_value = -EACCES;
        goto out;
    }

    if (!dev->guess) {
        dev->guess = kmalloc(guess_limit * sizeof(char *), GFP_KERNEL);
        if (!dev->guess)
            goto out;
        memset(dev->guess, 0, guess_limit * sizeof(char *));
    }
    if (!dev->guess[guess_count]) {
        dev->guess[guess_count] = kmalloc(guess_size * sizeof(char), GFP_KERNEL);
        if (!dev->guess[guess_count])
            goto out;
    }

    guess_buffer = kmalloc(length * sizeof(char), GFP_KERNEL);

    if (copy_from_user(guess_buffer, buf, guess_size)) {
        return_value = -EFAULT;
        goto out;
    }

    result_buffer = guess_handler(guess_buffer, mmind_number, guess_count + 1);

    dev->guess[guess_count] = result_buffer;
	
    dev->guess_count++;
    return_value = length;

	(*offset)++;
    kfree(guess_buffer);
  out:
    up(&dev->sem);
    printk(KERN_INFO "mastermind: Inside %s\n", __FUNCTION__);
    return return_value;
}

long mastermind_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    
    int err = 0;
	int retval = 0;
	
    /*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != MMIND_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > MMIND_IOC_MAXNR) return -ENOTTY;
	
    /*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;
	
	switch(cmd) {
        case MMIND_REMAINING:
			if (! capable (CAP_SYS_ADMIN))
				return -EPERM;
            retval = __put_user(mastermind_guess_limit - mastermind_devices->guess_count, (int __user *)arg);
	    break;
	 case MMIND_ENDGAME:
            clear_guesses(filp->private_data);
            break;
	 case MMIND_NEWGAME:
            mastermind_cleanup_module();
            
            if (copy_from_user(mmind_number, (char __user*)arg, sizeof(char*)))     // copying from user space to kernel space
				return -EFAULT;

            if(!mastermind_init_module())
                printk(KERN_INFO "Error: Initiation of module failed.\n");
            break;
    }
    return retval;
} 

int mastermind_close(struct inode *xnode, struct file *xfile) {
    printk(KERN_INFO "mastermind: Inside %s\n", __FUNCTION__);
    return 0;
}

struct file_operations mastermind_fops = {
    .owner =    THIS_MODULE,
    .open =     mastermind_open,	
    .read =     mastermind_read,
    .write =    mastermind_write,
    .unlocked_ioctl =  mastermind_ioctl,
    .release =  mastermind_close,
};

void mastermind_cleanup_module(void) {
    dev_t devno = MKDEV(mastermind_major, mastermind_minor);

    if (mastermind_devices) {
        mastermind_trim(mastermind_devices);
        cdev_del(&mastermind_devices[0].cdev);
        kfree(mastermind_devices);
    }

    printk(KERN_INFO "mastermind: Inside %s\n", __FUNCTION__);

    // Unregister the module
    unregister_chrdev_region(devno, 1);
}

int mastermind_init_module(void) {
    int result;
    int err;
    dev_t devno = 0;
    struct mastermind_dev *dev;

    // Register the module
    if (mastermind_major) {
        devno = MKDEV(mastermind_major, mastermind_minor);
        /*obtaining device number to work with statically */
        result = register_chrdev_region(devno, 1, "mastermind");
    } else {
    	/*obtaining device number to work with dinamycally*/
        result = alloc_chrdev_region(&devno, mastermind_minor, 1, "mastermind");
        mastermind_major = MAJOR(devno);
    }
    if (result < 0) {
        printk(KERN_WARNING "mastermind: can't get major %d\n", mastermind_major);
        return result;
    }

    mastermind_devices = kmalloc(sizeof(struct mastermind_dev), GFP_KERNEL);
    if (!mastermind_devices) {
        result = -ENOMEM;
        goto fail;
    }
    memset(mastermind_devices, 0, sizeof(struct mastermind_dev));

    // Initializion of the device
    dev = &mastermind_devices[0];
    dev->guess_size = MASTERMIND_GUESS_SIZE;
    if(mastermind_guess_limit >= 256) {
        printk(KERN_INFO "Error: Guess limit cannot be greater than 256.\n");
        result = -EACCES;
        goto fail;
    }
    
    dev->guess_limit = mastermind_guess_limit;
	dev->mmind_number = mmind_number;
    dev->guess_count = 0;
    sema_init(&dev->sem, 1);
    devno = MKDEV(mastermind_major, mastermind_minor );
    cdev_init(&dev->cdev, &mastermind_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &mastermind_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_INFO "Error: %d adding mastermind", err);
            
    printk(KERN_INFO "mastermind: Inside %s\n", __FUNCTION__);
    return 0;

  fail:
    mastermind_cleanup_module();
    return result;
}

module_init(mastermind_init_module);
module_exit(mastermind_cleanup_module);
