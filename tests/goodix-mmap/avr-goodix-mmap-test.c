// SPDX-License-Identifier: GPL-2.0-only
/* Exact production mapping/release methods, native VM and page references.
 * Core lifetime remains static in this fixture; no physical device is bound. */
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#define MMAP_BUFFER_SIZE (10 * 1024 * 20)
struct goodix_thp_core {
        struct miscdevice thp_misc_dev;
        struct mutex frame_mutex;
        struct { char *buf; } frame_mmap_list;
};
#include "generated.h"
static struct goodix_thp_core core;
static long drop_owner(struct file *file, unsigned int cmd, unsigned long arg)
{
        if (cmd != 1 || arg)
                return -EINVAL;
        goodix_thp_frame_buffer_release(&core);
        return 0;
}
static const struct file_operations ops = {
        .owner = THIS_MODULE, .mmap = goodix_thp_mmap, .unlocked_ioctl = drop_owner,
};
static int __init start(void)
{
        int ret;
        mutex_init(&core.frame_mutex);
        core.frame_mmap_list.buf = alloc_pages_exact(MMAP_BUFFER_SIZE, GFP_KERNEL | __GFP_ZERO);
        if (!core.frame_mmap_list.buf)
                return -ENOMEM;
        core.thp_misc_dev = (struct miscdevice){ .minor = MISC_DYNAMIC_MINOR,
                .name = "avr_goodix_mmap", .fops = &ops };
        ret = misc_register(&core.thp_misc_dev);
        if (ret)
                goodix_thp_frame_buffer_release(&core);
        return ret;
}
static void __exit stop(void)
{
        misc_deregister(&core.thp_misc_dev);
        goodix_thp_frame_buffer_release(&core);
}
module_init(start);module_exit(stop);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Real VM test of selected Goodix mapping source methods");
