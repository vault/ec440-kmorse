
#include "message_mod.h"



module_init(message_init);
module_exit(message_exit);

static int __init message_init(void)
{
    major = register_chrdev(0, DEV_NAME, &message_ops);
    valid = 0;

    if (major < 0) {
        printk(KERN_ALERT "Registering device failed with %d\n", major);
        return major;
    }

    printk(KERN_INFO "Loaded message module, major %d\n", major);
    return 0;
}

static void __exit message_exit(void)
{
    unregister_chrdev(major, DEV_NAME);
    if (valid)
        kfree(message_buffer);
    printk(KERN_INFO "Unloaded message module\n");
}

static int message_open(struct inode *inode, struct file *file)
{
    if (device_is_open)
        return -EBUSY;
    message_pos = 0;
    device_is_open++;
    return 0;
}

static int message_release(struct inode *inode, struct file *file)
{
    device_is_open--;
    return 0;
}

static ssize_t message_read(struct file *filp, char *buffer, size_t len, loff_t *off)
{
    int to_copy, copied;

    if (!valid)
        return -EPERM;

    if (message_pos >= message_length)
        return 0;

    to_copy = len>message_length ? message_length : len;
    copied = copy_to_user(buffer, message_buffer+message_pos, to_copy);
    if (copied) // we didn't finish copying
        return -EIO;

    message_pos += to_copy;
    return to_copy;
}

static ssize_t message_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    int copied;
    if (valid) {
        kfree(message_buffer);
        valid = 0;
    }
    message_buffer = kcalloc(len+1, sizeof(char), GFP_KERNEL);
    message_length = len+1;

    copied = copy_from_user(message_buffer, buff, len);
    if (copied) {
        kfree(message_buffer);
        return -EIO;
    }
    valid = 1;
    return len;
}

