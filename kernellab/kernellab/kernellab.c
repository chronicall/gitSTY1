/*
 * Kernellab
 */
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "pidinfo.h"

// Change to 1 to enable printk debug messages.
#define DEBUG 0

/* Part I */
static ssize_t kcurrent_write(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
    // Print the address that's kept in buf
    if(DEBUG)
    {
        printk("Address in buf : %p\n", (void *)(*((int *)buf)));
        printk("Current PID    : %d\n", current->pid);
    }

    // Copy the pid of the current process to user space, of course
    // checking for errors!
    if(copy_to_user((void *)(*((int *)buf)), &current->pid, sizeof(int)) != 0)
    {
        printk("copy_to_user didn't manage to copy.\n");
        return -EINVAL;
    }

  	return count;
}

static struct kobj_attribute kcurrent_attribute =
	__ATTR(kcurrent, 0222, NULL, kcurrent_write);


/* Part II */
static ssize_t pid_write(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
    int test_pid = -1;
    struct task_struct *task_list;
    struct sysfs_message *mess;
    struct pid_info *info;

    // Print the address that's kept in buf
    if(DEBUG)
    {
        printk("Address in buf : %p\n", (void *)(*((int *)buf)));
    }

    // Allocate memory for sysfs_message and pid_info.
    if((mess = kmalloc(sizeof(struct sysfs_message), GFP_KERNEL)) == NULL)
    {
        printk("sysfs_message allocation failed.\n");
        return -EINVAL;
    }
    
    if((info = kmalloc(sizeof(struct pid_info), GFP_KERNEL)) == NULL)
    {
        printk("pid_info allocation failed.\n");
        return -EINVAL;
    }

    // Copy the struct from user space, so we can work with it,
    // again, check for errors.
    if(copy_from_user(mess, (void *)(*((int *)buf)), sizeof(struct sysfs_message)) != 0)
    {
        printk("copy_from_user didn't manage to copy.\n");
        kfree(mess);
        kfree(info);
        return -EINVAL;
    }

    for_each_process(task_list)
    {
        // Find the right process. Fill the pid_info struct.
        if(task_list->pid == mess->pid)
        {
            test_pid = task_list->pid;
            info->pid = task_list->pid;
            // Should be safe to use sscanf, but better be safe than sorry, no?
            //sscanf(task_list->comm, "%s", info->comm);
            strncpy(info->comm, task_list->comm, 16);
            info->comm[15] = '\0';
            info->state = task_list->state;
            // If debug is enabled, print out the contents of info, to see
            // if it's what we want.
            if(DEBUG)
            {
                printk("info->pid   : %d  ---- task->pid   : %d\n", info->pid, task_list->pid);
                printk("info->comm  : %s  ---- task->comm  : %s\n", info->comm, task_list->comm);
                printk("info->state : %ld ---- task->state : %ld\n", info->state, task_list->state);
            }
            // Break out of the loop when we have what we want.
            break;
        }
    }

    if(test_pid == -1)
    {
        printk("Process not found.\n");
        kfree(mess);
        kfree(info);
        return -EINVAL;
    }
    // Copy the pid_info struct back to user space, with the address given in
    // the sysfs_message struct. Checking for errors, duh!
    if(copy_to_user(mess->address, info, sizeof(struct pid_info)) != 0)
    {
        printk("copy_to_user didn't manage to copy.\n");
        kfree(mess);
        kfree(info);
        return -EINVAL;
    }

    // Free up the memory.
    kfree(mess);
    kfree(info);

  	return count;
}

static struct kobj_attribute pid_attribute =
	__ATTR(pid, 0222, NULL, pid_write);

/* Setup  */
static struct attribute *attrs[] = {
	&kcurrent_attribute.attr,
	&pid_attribute.attr,
	NULL,
};
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *kernellab_kobj;

static int __init kernellab_init(void)
{
	int retval;
    struct task_struct *task_list;

    printk("kernellab module INJECTED\n");

    for_each_process(task_list)
    {
        if(task_list->pid == 1)
            printk("%s\n", task_list->comm);
    }

	kernellab_kobj = kobject_create_and_add("kernellab", kernel_kobj);
	if (!kernellab_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(kernellab_kobj, &attr_group);
	if (retval) 
		kobject_put(kernellab_kobj);
	return retval;
}

static void __exit kernellab_exit(void)
{
    struct task_struct *task_list;
    
    printk("kernellab module UNLOADED\n");
    
    for_each_process(task_list)
    {
        if(task_list->pid == 1)
            printk("%s\n", task_list->comm);
    }

	kobject_put(kernellab_kobj);
}

module_init(kernellab_init);
module_exit(kernellab_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernellab module, OS class at RU 2014!");
MODULE_AUTHOR("Sandra Ros Hrefnu Jonsdottir <sandrarj13@ru.is>");
MODULE_AUTHOR("Grettir Olafsson <grettir10@ru.is>");
