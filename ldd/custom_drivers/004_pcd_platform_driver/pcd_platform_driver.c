#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "platform.h"

#define MAX_DEV_SUPPORT 10

#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt, __func__

/* Device Private Data Structure */
struct pcdev_priv_data
{
	struct pcdev_platform_data pdata;
	char * buffer;
	dev_t dev_num;
	struct cdev cdev;
};

/* Driver Private Data Structure */
struct pcdrv_priv_data
{
	int total_devs;
	dev_t device_num_base;
	struct class *class_pcd;
	struct device *device_pcd;
};

struct pcdrv_priv_data pcdrv_data;

loff_t pcd_lseek (struct file * filp, loff_t off, int whence)
{
	
	struct pcdev_priv_data * pcdev_d = (struct pcdev_priv_data *)filp->private_data;	
	int max_size = pcdev_d->pdata.size;

	loff_t temp;
	pr_info("lseek requested \n");
	pr_info("Current value of file position = %lld\n", filp->f_pos);
	switch(whence)
	{
		case SEEK_SET:
			if ((off > max_size) || (off < 0))
				return -EINVAL;
			filp->f_pos = off;
			break;
		case SEEK_CUR:
			temp = filp->f_pos + off;
			if ((temp > max_size) || (temp < 0))
				return -EINVAL;
			filp->f_pos = temp;
			break;
		case SEEK_END:
			temp = max_size + off;
			if ((temp > max_size) || (temp < 0))
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
	struct pcdev_priv_data * pcdev_d = (struct pcdev_priv_data *)filp->private_data;	
	int max_size = pcdev_d->pdata.size;

	pr_info("read requested for %zu bytes \n", count);
	pr_info("Current file position = %lld\n", *f_pos);
	
	/* Adjust the count */
	if((*f_pos + count) > max_size)
		count = max_size - *f_pos;

	/* Copy to User */
	if(copy_to_user(buff, pcdev_d->buffer + (*f_pos), count))
		return -EFAULT;

	/* Update the current file position */
	*f_pos += count;

	pr_info("Number of bytes successfully read = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	return count;
}


ssize_t pcd_write (struct file * filp, const char __user * buff, size_t count, loff_t * f_pos)
{
	struct pcdev_priv_data * pcdev_d = (struct pcdev_priv_data *)filp->private_data;	
	int max_size = pcdev_d->pdata.size;
	
	pr_info("write requested for %zu bytes \n", count);
	pr_info("Current file position = %lld\n", *f_pos);
	
	/* Adjust the count */
	if((*f_pos + count) > max_size)
		count = max_size - *f_pos;
	
	if(!count)
	{
		pr_err("No space left on the device buffer");
		return -ENOMEM;
	}
	
	/* Copy to User */
	if(copy_from_user(pcdev_d->buffer + (*f_pos), buff, count))
		return -EFAULT;

	/* Update the current file position */
	*f_pos += count;

	pr_info("Number of bytes successfully written = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	/* Return the number of bytes written */	
	return count;
}

int check_permission (int dev_perm, int acc_mode)
{
	if (dev_perm == RDWR)
		return 0;
	if ( (dev_perm == RDONLY) && ( (acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE) ) )
		return 0;
	if ( (dev_perm == WRONLY) && ( !(acc_mode & FMODE_READ) && (acc_mode & FMODE_WRITE) ) )
		return 0;
	
	return -ENOMEM;
}

int pcd_open (struct inode * inode, struct file * filp)
{
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

/* Gets called when the device is removed from the system  */
int pcd_platform_driver_remove( struct platform_device *pdev )
{

	struct pcdev_priv_data *dev_data = dev_get_drvdata(&pdev->dev);
	/* 1. Remove a device that was created with device_create() */
	device_destroy(pcdrv_data.class_pcd, dev_data->dev_num);

	/* 2. Remove a cdev entry form the system */
	cdev_del(&dev_data->cdev);

	pcdrv_data.total_devs--;
	
	pr_info("device is removed \n");
	return 0;
}

/* Gets called when matched platform device is found */
int pcd_platform_driver_probe( struct platform_device *pdev )
{
	int ret;
	struct pcdev_priv_data *dev_data;
	struct pcdev_platform_data *pdata;
	
	pr_info("Device is detected\n");

	/* 1. Get the platform data */
	pdata = (struct pcdev_platform_data *) dev_get_platdata(&pdev->dev);		
	if (!pdata) {
		pr_info("No platform data available \n");
		ret = -EINVAL;
		goto out;
	}

	/* 2. Dynamically allocate memory for the device private data */
	dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
	if(!dev_data) {
		pr_info("Cannot allocate memory \n");
		ret = -ENOMEM;
		goto out;
	}

	/* Save the device private data pointer in the platform device structure  */
	dev_set_drvdata(&pdev->dev, dev_data);

	dev_data->pdata.size = pdata->size;
	dev_data->pdata.perm = pdata->perm;
	dev_data->pdata.serial_number = pdata->serial_number;
	
	pr_info("Device serial number = %s\n", dev_data->pdata.serial_number);
	pr_info("Device size = %d\n", dev_data->pdata.size);
	pr_info("Device permissions = %d\n", dev_data->pdata.perm);

	/* 3. Dynamically allocate memory for the device buffer using size 
	information from the platform data  */
	dev_data->buffer = devm_kzalloc(&pdev->dev, dev_data->pdata.size, GFP_KERNEL);
	if(!dev_data->buffer) {
		pr_info("Cannot allocate memory \n");
		ret = -ENOMEM;
		goto dev_data_free;
	}

	/* 4. Get the device number */
	dev_data->dev_num = pcdrv_data.device_num_base + pdev->id;

	/* 5. Do cdev init and cdev add */
	cdev_init(&dev_data->cdev, &pcd_fops);
	dev_data->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
	if(ret < 0) {
		pr_err("cdev add failed \n");
		goto buffer_free;
	}

	/* 6. Create device file for the detected platform device */
	pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, dev_data->dev_num, NULL, "pcdev-%d", pdev->id);
	if (IS_ERR(pcdrv_data.device_pcd))
	{
        	pr_err("device creation failed!\n");
        	ret = PTR_ERR(pcdrv_data.device_pcd);
        	goto cdev_del;
 	}	

	pcdrv_data.total_devs++;

	pr_info("Probe was successful \n");
	return 0;
	
	/* 7. Error handling  */
cdev_del:
	cdev_del(&dev_data->cdev);
buffer_free:
	devm_kfree(&pdev->dev, dev_data->buffer);
dev_data_free:
	devm_kfree(&pdev->dev, dev_data);

out:
	pr_info("Device probe failed\n");
	return ret;		

}

struct platform_driver pcd_platform_driver = 
{
	.probe = pcd_platform_driver_probe,
	.remove = pcd_platform_driver_remove,
	.driver = {
		.name = "pseudo-char-device"
	}	
};

static int __init pcd_platform_driver_init(void)
{
	int ret;
	/* 1. Dynamically allocate a device number */
	ret = alloc_chrdev_region(&pcdrv_data.device_num_base, 0, MAX_DEV_SUPPORT, "pcd_devices");
	if (ret < 0)
	{
		pr_err("allocate char device region failed!\n");
		return ret;
	}
	
	/* 2. create device class under /sys/class/ */
	pcdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(pcdrv_data.class_pcd))
	{
		pr_err("class creation failed!\n");
		ret = PTR_ERR(pcdrv_data.class_pcd);
		unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEV_SUPPORT);
		return ret;
	}
	
	/* 3. Register platform driver  */
	platform_driver_register (&pcd_platform_driver);
	pr_info("pcd platform driver loaded \n");
	return 0;
}

static void __exit pcd_platform_driver_cleanup(void)
{
	/* 1. Unregister the platform driver */
	platform_driver_unregister (&pcd_platform_driver);
	
	/* 2. Class Destroy */
	class_destroy(pcdrv_data.class_pcd);

	/* 3. Unregister device numbers for MAX_DEV_SUPPORT */
	unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEV_SUPPORT);

	pr_info("pcd platform driver unloaded \n");
}

module_init(pcd_platform_driver_init);
module_exit(pcd_platform_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ME");
MODULE_DESCRIPTION("Pseudo character platform driver supporting N devices");
