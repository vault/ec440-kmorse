
#include "morse_mod.h"

module_init(message_init);
module_exit(message_exit);

static int __init message_init(void)
{
    int err, i;
    struct message *msg;
    dev_t devno;
    dev_class = class_create(THIS_MODULE, "morse_mod");
    alloc_chrdev_region(&device, 0, message_count, DEV_NAME);
    major = MAJOR(device);

    int g = gpio_request(PIN);
    if (g != 0) {
        return -EINVAL;
    }
    gpio_direction_output(PIN, 0);

    message_list = kcalloc(sizeof(struct message*), message_count, GFP_KERNEL);
    for (i = 0; i < message_count; i++) {
        devno = MKDEV(major, i);
        msg = new_message(i);
        message_list[i] = msg;

        err = cdev_add(&msg->dev, devno, 1);
        if (err)
            printk(KERN_NOTICE"Error %d adding message%d", err, i);
        device_create(dev_class, NULL, devno, NULL, "morse%d", i);
    }

    printk(KERN_INFO"Loaded module: morse_mod\n");
    return 0;
}

static void __exit message_exit(void)
{
    int i;
    dev_t devno;
    for (i = 0; i < message_count; i++) {
        devno = MKDEV(major, i);
        device_destroy(dev_class, message_list[i]->dev.dev);
        cdev_del(&message_list[i]->dev);
        kfree(message_list[i]);
    }
    kfree(message_list);
    class_destroy(dev_class);
    unregister_chrdev_region(device, message_count);
    printk(KERN_INFO "Unloaded module: morse_mod\n");
}

static char* morse_char(char x) {
    if (x >= 'a' && x <= 'z') { // Convert to uppercase
        x -= ('a' - 'A');
    }

    // If it's not a letter a number
    if (!((x >= '0' && x <= '9') || (x >= 'A' && x <= 'Z'))) {
        return NULL; // non coded will be treated as a space
    } else {
        // If it's a number, calculate position
        if (x >= '0' && x <= '9') {
            return morse_code[x-'0'+26];
        } else { // Or look up digit
            return morse_code[x-'A'];
        }
    }
}

static struct message* new_message(int minor)
{
    struct message *msg = kmalloc(sizeof(struct message), GFP_KERNEL);
    int i;

    for (i = 0; i < 32; i++)
        msg->buffer[i] = 0;
    cdev_init(&msg->dev, &message_ops);
    msg->dev.owner = THIS_MODULE;
    msg->minor = minor;
    msg->open = 0;
    msg->pos = 0;
    msg->len = 0;
    msg->valid = 0;
    return msg;
}

static void step_display(unsigned long arg)
{
    unsigned long delay;
    const unsigned int time_unit = HZ/2; // Half a second
    char *code = morse_char(morse.msg->buffer[morse.msg_pos]);

    if (morse.state == 0 && code != NULL) {

        gpio_set_value(PIN, 1);
        printk(KERN_INFO"LED ON: %c, letter %c\n", 
                code[morse.l_pos], morse.msg->buffer[morse.msg_pos]);

        morse.state = 1;
        if (code[morse.l_pos] == '-')
            delay = 3 * time_unit;
        else
            delay = 1 * time_unit;

    } else if (code == NULL) {
        gpio_set_value(PIN, 0);
        printk(KERN_INFO"LED DELAY: letter %c\n", morse.msg->buffer[morse.msg_pos]);

        morse.state = 0;
        morse.msg_pos++;
        morse.l_pos = 0;
        delay = 7 * time_unit;

        if (morse.msg_pos >= morse.msg->len)
            morse.done = 1;

    } else {
        gpio_set_value(PIN, 0);

        printk(KERN_INFO"LED OFF: %c, letter %c\n", 
                code[morse.l_pos], morse.msg->buffer[morse.msg_pos]);

        morse.state = 0;
        morse.l_pos++;

        // If we've finished this letter
        if (code[morse.l_pos] == '\0') {
            morse.l_pos = 0;
            morse.msg_pos++;
            delay = 3 * time_unit;
        } else {
            delay = 1 * time_unit;
        }

        // If we've finished the messaeg
        if (morse.msg_pos >= morse.msg->len)
            morse.done = 1;
    }

    // Rebuild timer to display next
    del_timer(&timer);
    if (!morse.done) {
        init_timer(&timer);
        timer.expires = jiffies + delay;
        timer.data = 0;
        timer.function = step_display;
        add_timer(&timer);
    } else {
        morse.msg->open--;
        printk(KERN_INFO"Finished Morse\n");
    }
}

static void start_morse(int msg_idx)
{
    // Re-initialize our display struct
    morse.msg = message_list[msg_idx];
    morse.msg->open++;
    morse.msg_pos = 0;
    morse.l_pos = 0;
    morse.done = 0;
    morse.state = 0;

    init_timer(&timer);
    timer.expires = jiffies + (HZ/2);
    timer.data = 0;
    timer.function = step_display;
    add_timer(&timer);
}

static int message_open(struct inode *inode, struct file *filp)
{
    int minor = MINOR(inode->i_rdev);
    struct message *msg = message_list[minor];

    min_data min;
    min.minor = minor;

    if (msg->open)
        return -EBUSY;

    msg->open++;
    msg->pos = 0;
    filp->private_data = min.pt;
    return 0;
}

static int message_release(struct inode *inode, struct file *filp)
{
    struct message *msg = message_list[((min_data)filp->private_data).minor];
    if (msg != NULL) {
        msg->pos = 0;
        msg->open--;
    }
    return 0;
}

static ssize_t message_read(struct file *filp, char *buffer, size_t len, loff_t *off)
{
    int to_copy, left;
    struct message *msg = message_list[((min_data)filp->private_data).minor];

    if (!msg->valid)
        return -EINVAL;

    if (msg->pos >= msg->len)
        return 0;

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
    struct message *msg = message_list[((min_data)filp->private_data).minor];

    if (msg->valid) {
        msg->valid = 0;
    }

    to_copy = len > 31 ? 31 : len;
    copy_from_user(msg->buffer, buff, to_copy);

    msg->len = to_copy;
    msg->buffer[len+1] = '\0';

    if (to_copy == 0)
        msg->buffer[0] = '\0';

    msg->valid = 1;

    start_morse(((min_data)filp->private_data).minor);
    return len;
}

