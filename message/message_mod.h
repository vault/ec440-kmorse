
#ifndef MESSAGE_MOD
#define MESSAGE_MOD

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#define DEV_NAME "message"
#define AUTHOR "Michael Abed <michaelabed@gmail.com>"
#define DESC "A device that holds a message"
#define LICENSE "GPL"

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESC);
MODULE_LICENSE(LICENSE);
MODULE_SUPPORTED_DEVICE(DEV_NAME);

static int __init message_init(void);
static void __exit message_exit(void);

static short message_count = 1;
module_param(message_count, short, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(message_count, "Number of message devices to create");

static int message_open(struct inode *inode, struct file *file);
static int message_release(struct inode *inode, struct file *file);
static ssize_t message_read(struct file *filp, char *buffer, size_t len, loff_t *off);
static ssize_t message_write(struct file *filp, const char *buffer, size_t length, loff_t *off);


static struct file_operations message_ops = {
    .owner = THIS_MODULE,
    .read = message_read,
    .write = message_write,
    .open = message_open,
    .release = message_release
};

static int major;
static dev_t device;
static struct class *dev_class;

struct message {
    char buffer[32];
    struct cdev dev;
    int minor;
    int open;
    int pos;
    int len;
    int valid;
    //struct list_head list;
};

union min_data {
    void *pt;
    int minor;
};

typedef union min_data min_data;

static struct message *new_message(int minor);

static struct message** message_list;

#endif

