
#ifndef MESSAGE_MOD
#define MESSAGE_MOD

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#define DEV_NAME "message"
#define AUTHOR "Michael Abed <michaelabed@gmail.com>"
#define DESC "A device that holds a message"
#define LICENSE "DUAL MIT/GPL"

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESC);
MODULE_LICENSE(LICENSE);
MODULE_SUPPORTED_DEVICE(DEV_NAME);

static int __init message_init(void);
static void __exit message_exit(void);

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

static int device_is_open = 0;
static int major;

static char* message_buffer;
static int message_length;
static int valid;

#endif

