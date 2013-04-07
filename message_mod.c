
#include <linux/module.h>
#include <linux/kernel.h>

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello world 1.\n");

    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye wold 1.\n");
}

module_init(hello_init);
module_exit(hello_exit);

