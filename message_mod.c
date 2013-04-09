
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
        list_del(pos);
        kfree(tmp);
    }
    printk(KERN_INFO "Unloaded message module\n");
}

static struct message* new_message(int minor)
{
    struct message *msg = kmalloc(sizeof(struct message), GFP_KERNEL);
    int i;

    printk(KERN_INFO"message: creating message for minor %d\n", minor);

    for (i = 0; i < 32; i++)
        msg->buffer[i] = 0;
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

    printk(KERN_INFO"message: searching for minor %d\n", minor);

    if (message_list->minor == minor)
        return message_list;
    list_for_each(pos, &message_list->list) {
        tmp = list_entry(pos, struct message, list);
        printk(KERN_INFO"message: minor %d\n", tmp->minor);
        if (tmp->minor == minor)
            return tmp;
    }
    return NULL;
}

static void add_message(struct message *msg)
{
    printk(KERN_INFO"message: adding message for minor %d\n", msg->minor);
    list_add(&msg->list, &message_list->list);
}

static int message_open(struct inode *inode, struct file *filp)
{
    int minor = MINOR(inode->i_rdev);
    struct message *msg;

    min_data min;
    min.minor = minor;

    printk(KERN_INFO"message: opened minor %d\n", minor);

    // Find or allocate the struct message for this minor
    if (message_list == NULL) {
        printk(KERN_INFO"message: new message_list\n");
        message_list = new_message(minor);
        msg = message_list;
        INIT_LIST_HEAD(&message_list->list);
    } else {
        printk(KERN_INFO"message: looking for message\n");
        msg = find_message(minor);
    }
    if (msg == NULL) {
        printk(KERN_INFO"message: not found, allocing new\n");
        msg = new_message(minor);
        add_message(msg);
    }

    if (msg->open)
        return -EINVAL;

    msg->open++;
    msg->pos = 0;
    filp->private_data = min.pt;
    printk(KERN_INFO"message: finished open\n");
    return 0;
}

static int message_release(struct inode *inode, struct file *filp)
{
    struct message *msg = find_message(((min_data)filp->private_data).minor);
    if (msg != NULL) {
        printk(KERN_INFO"message: closing message %d\n", msg->minor);
        msg->pos = 0;
        msg->open--;
    }
    return 0;
}

static ssize_t message_read(struct file *filp, char *buffer, size_t len, loff_t *off)
{
    int to_copy, left;
    struct message *msg = find_message(((min_data)filp->private_data).minor);
    if (msg == NULL)
        return -EINVAL;

    if (!msg->valid)
        return -EINVAL;

    if (msg->pos >= msg->len)
        return 0;

    printk(KERN_INFO"message: reading from %d message \"%s\"\n", msg->minor, msg->buffer);

    to_copy = len > msg->len ? msg->len : len;
    left = copy_to_user(buffer, msg->buffer+msg->pos, to_copy);
    if (left) // we didn't finish copying
        return -EINVAL;

    msg->pos += to_copy;
    return to_copy;
}

static ssize_t message_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    int to_copy;
    struct message *msg = find_message(((min_data)filp->private_data).minor);
    if (msg == NULL) {
        return -EINVAL;
    }

    if (msg->valid) {
        msg->valid = 0;
    }

    to_copy = len > 31 ? 31 : len;
    copy_from_user(msg->buffer, buff, to_copy);

    printk(KERN_INFO"message: writing to %d \"%s\"\n", msg->minor, msg->buffer);
    msg->len = to_copy;
    msg->buffer[len+1] = '\0';

    msg->valid = 1;
    return to_copy;
}

