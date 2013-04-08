
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

    printk(KERN_INFO "Loaded message module\n");
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
    copy_to_user(buffer, message_buffer, len);
    if (len < message_length)
        (*off)+=len;
    return len;
}

static ssize_t message_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    if (valid) {
        kfree(message_buffer);
        valid = 0;
    }
    message_buffer = kcalloc(len, sizeof(char), GFP_KERNEL);
    message_length = len;
    valid = 1;
    copy_from_user(message_buffer, buff, len);
    return len;
}

