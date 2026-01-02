#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>

#define DEV_MEM_SIZE 512

#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt, __func__

/* pseudo device's memory */
char device_buffer[DEV_MEM_SIZE];

/* Device number  */
dev_t device_number;

/* cdev variable */
struct cdev pcd_cdev;

loff_t pcd_lseek (struct file * filp, loff_t off, int whence)
{
	loff_t temp;
	pr_info("lseek requested \n");
	pr_info("Current value of file position = %lld\n", filp->f_pos);
	switch(whence)
	{
		case SEEK_SET:
			if ((off > DEV_MEM_SIZE) || (off < 0))
				return -EINVAL;
			filp->f_pos = off;
			break;
		case SEEK_CUR:
			temp = filp->f_pos + off;
			if ((temp > DEV_MEM_SIZE) || (temp < 0))
				return -EINVAL;
			filp->f_pos = temp;
			break;
		case SEEK_END:
			temp = DEV_MEM_SIZE + off;
			if ((temp > DEV_MEM_SIZE) || (temp < 0))
				return -EINVAL;		
			filp->f_pos = temp;
			break;
		default:
			return -EINVAL;
	}
	pr_info("Updated value of file position = %lld\n", filp->f_pos);
	return filp->f_pos;
}

ssize_t pcd_read (struct file * filp, char __user * buff, size_t count, loff_t * f_pos)
{
	pr_info("read requested for %zu bytes \n", count);
	pr_info("Current file position = %lld\n", *f_pos);
	
	/* Adjust the count */
	if((*f_pos + count) > DEV_MEM_SIZE)
		count = DEV_MEM_SIZE - *f_pos;

	/* Copy to User */
	if(copy_to_user(buff, &device_buffer[*f_pos], count))
		return -EFAULT;

	/* Update the current file position */
	*f_pos += count;

	pr_info("Number of bytes successfully read = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	return count;
}

ssize_t pcd_write (struct file * filp, const char __user * buff, size_t count, loff_t * f_pos)
{
	pr_info("write requested for %zu bytes \n", count);
	pr_info("Current file position = %lld\n", *f_pos);
	
	/* Adjust the count */
	if((*f_pos + count) > DEV_MEM_SIZE)
		count = DEV_MEM_SIZE - *f_pos;
	
	if(!count)
	{
		pr_err("No space left on the device buffer");
		return -ENOMEM;
	}
	
	/* Copy to User */
	if(copy_from_user(&device_buffer[*f_pos], buff, count))
		return -EFAULT;

	/* Update the current file position */
	*f_pos += count;

	pr_info("Number of bytes successfully written = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	/* Return the number of bytes written */	
	return count;
}

int pcd_open (struct inode * inode, struct file * filp)
{
	pr_info("open was successful \n");
	return 0;
}

int pcd_release (struct inode * inode, struct file * filp)
{
	pr_info("release was successful \n");
	return 0;
}

/* file operations of the pcd driver */
struct file_operations pcd_fops = 
{
	.open = pcd_open,
        .read = pcd_read,
	.write = pcd_write,
	.llseek = pcd_lseek,
	.release = pcd_release,
	.owner = THIS_MODULE
};

struct class * class_pcd;
struct device * device_pcd;

static int __init pcd_driver_init(void)
{
	int ret;

	/* 1. Dynamically allocate a device number */
	ret = alloc_chrdev_region(&device_number, 0, 1, "pcd_devices");
	if (ret < 0)
	{
		pr_err("Allocate Char Device region failed!\n");
		goto out;
	}
	pr_info("Device number <major>:<minor> = %d:%d\n", MAJOR(device_number), MINOR(device_number));
	
	/* 2. Initialize the cdev structure with fops */
	cdev_init(&pcd_cdev, &pcd_fops);
	
	/* 3. Registration of the cdev structure with VFS */
        pcd_cdev.owner = THIS_MODULE;
	ret = cdev_add(&pcd_cdev, device_number, 1);
	if(ret < 0)
	{
		pr_err("Char Device add failed!\n");
		goto unreg_chrdev;
	}
	/* 4. create device class under /sys/class/ */
	class_pcd = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(class_pcd))
	{
		pr_err("Class creation failed!\n");
		ret = PTR_ERR(class_pcd);
		goto cdev_del;
	}

	/* 5. populate the sysfs with device information  */
	device_pcd = device_create(class_pcd, NULL, device_number, NULL, "pcd");
        if (IS_ERR(device_pcd))
        {
                pr_err("Device creation failed!\n");
                ret = PTR_ERR(device_pcd);
                goto class_del;
        }

	pr_info("Module Init was succesful!\n");
	return 0;

class_del:
	class_destroy(class_pcd);

cdev_del:
	cdev_del(&pcd_cdev);

unreg_chrdev:
	unregister_chrdev_region(device_number, 1);

out:
	pr_info("Module Insetion failed!\n");
	return ret;
}


static void __exit pcd_driver_cleanup(void)
{
	/* device destroy */
	device_destroy(class_pcd, device_number);

	/* class destroy */
	class_destroy(class_pcd);

	/* cdev delete */
	cdev_del(&pcd_cdev);

	/* unregister chrdev region */
	unregister_chrdev_region(device_number, 1);

	pr_info("Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ME");
MODULE_DESCRIPTION("Pseudo Character Driver");

