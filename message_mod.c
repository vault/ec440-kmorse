
#include "message_mod.h"

module_init(message_init);
module_exit(message_exit);

static int __init message_init(void)
{
    major = register_chrdev(0, DEV_NAME, &message_ops);

    if (major < 0) {
        printk(KERN_ALERT "Registering device failed with %d\n", major);
        return major;
    }

    printk(KERN_INFO "Loaded message module, major %d\n", major);
    return 0;
}

static void __exit message_exit(void)
{
    struct list_head *pos;
    struct list_head *pos2;
    struct message *tmp;

    unregister_chrdev(major, DEV_NAME);

    list_for_each_safe(pos, pos2, &message_list->list) {
        tmp = list_entry(pos, struct message, list);
        __list_del_entry(pos);
        kfree(tmp);
    }
    printk(KERN_INFO "Unloaded message module\n");
}

static struct message* new_message(int minor)
{
    struct message *msg = kmalloc(sizeof(struct message), GFP_KERNEL);
    msg->minor = minor;
    msg->open = 0;
    msg->pos = 0;
    msg->len = 0;
    msg->valid = 0;
    return msg;
}

static struct message *find_message(int minor)
{
    struct list_head *pos;
    struct message *tmp;
    list_for_each(pos, &message_list->list) {
        tmp = list_entry(pos, struct message, list);
        if (tmp->minor == minor)
            return tmp;
    }
    return NULL;
}

static void add_message(struct message *msg)
{
    list_add(&msg->list, &message_list->list);
}

static int message_open(struct inode *inode, struct file *filp)
{
    int minor = MINOR(inode->i_rdev);
    struct message *msg;

    min_data min;
    min.minor = minor;

    // Find or allocate the struct message for this minor
    if (message_list == NULL) {
        message_list = new_message(minor);
        msg = message_list;
        INIT_LIST_HEAD(&message_list->list);
    } else {
        msg = find_message(minor);
    }
    if (msg == NULL) {
        msg = new_message(minor);
        add_message(msg);
    }

    if (msg->open)
        return -EINVAL;

    msg->open++;
    msg->pos = 0;
    filp->private_data = min.pt;
    return 0;
}

static int message_release(struct inode *inode, struct file *filp)
{
    struct message *msg = find_message(((min_data)filp->private_data).minor);
    if (msg != NULL) {
        msg->pos = 0;
        msg->open--;
    }
    return 0;
}

static ssize_t message_read(struct file *filp, char *buffer, size_t len, loff_t *off)
{
    struct message *msg = find_message(((min_data)filp->private_data).minor);
    int to_copy, copied;

    if (!msg->valid)
        return -EINVAL;

    if (msg->pos >= msg->len)
        return 0;

    to_copy = len>msg->len ? msg->len : len;
    copied = copy_to_user(buffer, msg->buffer+msg->pos, to_copy);
    if (copied) // we didn't finish copying
        return -EINVAL;

    msg->pos += to_copy;
    return to_copy;
}

static ssize_t message_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    struct message *msg = find_message(((min_data)filp->private_data).minor);
    int copied;
    if (msg->valid) {
        msg->valid = 0;
    }

    copied = copy_from_user(msg->buffer, buff, 31);
    if (copied) {
        return -EINVAL;
    }

    msg->valid = 1;
    return len;
}

