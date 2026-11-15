#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/mmi_relay.h>
#include <linux/slab.h>
#include <linux/srcu.h>

typedef struct _relay_node {
    struct list_head list;
    union {
        struct atomic_notifier_head atomic_head;
        struct blocking_notifier_head blocking_head;
    };
    int id;
    notifier_t type;
    notifier_d dev;
}relay_node;

struct mmi_relay_dev {
    struct platform_device *pdev;
    struct mutex lock;
    relay_node nodes[NUM_TYPES][NUM_DEVS];
    unsigned notifier_nums;
};
static struct mmi_relay_dev __rcu *relay_dev;
static DEFINE_MUTEX(relay_provider_lock);
DEFINE_STATIC_SRCU(relay_provider_srcu);

enum relay_operation { RELAY_FIRE, RELAY_REGISTER, RELAY_UNREGISTER };

static int relay_call(struct mmi_relay_dev *relay, notifier_t type,
                      notifier_d dev, unsigned long val, void *data)
{
    if (type == ATOMIC)
        return atomic_notifier_call_chain(&relay->nodes[type][dev].atomic_head,
                                          val, data);
    return blocking_notifier_call_chain(&relay->nodes[type][dev].blocking_head,
                                        val, data);
}

static int relay_action(notifier_t type, notifier_d dev,
                        struct notifier_block *nb, unsigned long val, void *data,
                        enum relay_operation operation)
{
    struct mmi_relay_dev *relay;
    bool missing;
    int index, ret;

    if ((unsigned int)type >= NUM_TYPES || (unsigned int)dev >= NUM_DEVS)
        return -EPERM;
    if (operation != RELAY_FIRE && (!nb || !nb->notifier_call))
        return -EINVAL;

    index = srcu_read_lock(&relay_provider_srcu);
    relay = srcu_dereference(relay_dev, &relay_provider_srcu);
    missing = !relay;
    if (missing) {
        ret = -EINVAL;
        goto out;
    }
    if (operation == RELAY_FIRE) {
        ret = relay_call(relay, type, dev, val, data);
        goto out;
    }

    /* Each real notifier head serializes itself. Never hold a provider mutex
     * across callbacks or native notifier unregister's drain operation. */
    if (type == ATOMIC) {
        struct atomic_notifier_head *head = &relay->nodes[type][dev].atomic_head;
        ret = operation == RELAY_REGISTER ?
            atomic_notifier_chain_register(head, nb) :
            atomic_notifier_chain_unregister(head, nb);
    } else {
        struct blocking_notifier_head *head = &relay->nodes[type][dev].blocking_head;
        ret = operation == RELAY_REGISTER ?
            blocking_notifier_chain_register(head, nb) :
            blocking_notifier_chain_unregister(head, nb);
    }
    if (!ret)
        relay_call(relay, type, MMI_RELAY,
                   operation == RELAY_REGISTER ? RELAY_NOTIFY_REGISTER :
                                                RELAY_NOTIFY_UNREGISTER, NULL);
out:
    srcu_read_unlock(&relay_provider_srcu, index);
    /* A removed provider may still be draining a previously started callback.
     * Even a missing-provider unregister must wait before caller data is freed. */
    if (missing && operation == RELAY_UNREGISTER)
        synchronize_srcu(&relay_provider_srcu);
    return ret;
}

int relay_notifier_fire(notifier_t type, notifier_d dev, unsigned long val, void *data)
{
    return relay_action(type, dev, NULL, val, data, RELAY_FIRE);
}
EXPORT_SYMBOL_GPL(relay_notifier_fire);

int relay_register_action(notifier_t type, notifier_d dev, struct notifier_block *nb)
{
    return relay_action(type, dev, nb, 0, NULL, RELAY_REGISTER);
}
EXPORT_SYMBOL_GPL(relay_register_action);

int relay_unregister_action(notifier_t type, notifier_d dev, struct notifier_block *nb)
{
    return relay_action(type, dev, nb, 0, NULL, RELAY_UNREGISTER);
}
EXPORT_SYMBOL_GPL(relay_unregister_action);

static int mmi_relay_probe(struct platform_device *pdev)
{
    struct mmi_relay_dev *relay;
    unsigned int i, j;

    relay = kzalloc(sizeof(*relay), GFP_KERNEL);
    if (!relay)
        return -ENOMEM;
    relay->pdev = pdev;
    relay->notifier_nums = NUM_DEVS * NUM_TYPES;
    mutex_init(&relay->lock);
    for (i = 0; i < NUM_TYPES; i++) {
        for (j = 0; j < NUM_DEVS; j++) {
            relay->nodes[i][j].type = i;
            relay->nodes[i][j].dev = j;
            relay->nodes[i][j].id = i << 16 | j;
            INIT_LIST_HEAD(&relay->nodes[i][j].list);
            if (i == ATOMIC)
                ATOMIC_INIT_NOTIFIER_HEAD(&relay->nodes[i][j].atomic_head);
            else
                BLOCKING_INIT_NOTIFIER_HEAD(&relay->nodes[i][j].blocking_head);
        }
    }
    mutex_lock(&relay_provider_lock);
    if (rcu_access_pointer(relay_dev)) {
        mutex_unlock(&relay_provider_lock);
        kfree(relay);
        return -EBUSY;
    }
    platform_set_drvdata(pdev, relay);
    rcu_assign_pointer(relay_dev, relay);
    mutex_unlock(&relay_provider_lock);
    return 0;
}

static void mmi_relay_remove(struct platform_device *pdev)
{
    struct mmi_relay_dev *relay = platform_get_drvdata(pdev);

    mutex_lock(&relay_provider_lock);
    if (rcu_access_pointer(relay_dev) == relay)
        RCU_INIT_POINTER(relay_dev, NULL);
    platform_set_drvdata(pdev, NULL);
    mutex_unlock(&relay_provider_lock);
    synchronize_srcu(&relay_provider_srcu);
    kfree(relay);
}

static const struct of_device_id mmi_relay_mt[] = {
	{.compatible = "mmi,sys-relay"},
	{},
};
MODULE_DEVICE_TABLE(of, mmi_relay_mt);

static struct platform_driver mmi_relay_driver = {
	.probe = mmi_relay_probe,
	.remove = mmi_relay_remove,
	.driver = {
		.name = "mmi_relay",
		.owner = THIS_MODULE,
		.of_match_table = mmi_relay_mt,
	},
};

static int __init mmi_relay_init(void)
{
	return platform_driver_register(&mmi_relay_driver);
}

static void __exit mmi_relay_exit(void)
{
	platform_driver_unregister(&mmi_relay_driver);
}

module_init(mmi_relay_init);
module_exit(mmi_relay_exit);

MODULE_ALIAS("platform:mmi_relay");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Motorola Mobility Module Conmmunication Relay");
MODULE_LICENSE("GPL");
