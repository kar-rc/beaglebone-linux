#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "platform.h"

#define MAX_DEV_SUPPORT 10

#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt, __func__

struct device_config
{
	int config_item1;
	int config_item2;
};

enum pcdev_names
{
	PCDEVA1X,
	PCDEVB1X,
	PCDEVC1X,
	PCDEVD1X
};

struct device_config pcdev_config[] = 
{
	[PCDEVA1X] = {.config_item1 = 100, .config_item2 = 001},
	[PCDEVB1X] = {.config_item1 = 200, .config_item2 = 002},
	[PCDEVC1X] = {.config_item1 = 300, .config_item2 = 003},
	[PCDEVD1X] = {.config_item1 = 400, .config_item2 = 004},
};

struct platform_device_id pcdev_ids[] = 
{
	{ .name = "pcdev-A1x", .driver_data = PCDEVA1X },
	{ .name = "pcdev-B1x", .driver_data = PCDEVB1X },
	{ .name = "pcdev-C1x", .driver_data = PCDEVC1X },
	{ .name = "pcdev-D1x", .driver_data = PCDEVD1X },
	{ }
};

struct of_device_id org_pcdev_dt_match[] = 
{
	{ .compatible = "pcdev-A1x", .data = (void *)PCDEVA1X },
	{ .compatible = "pcdev-B1x", .data = (void *)PCDEVB1X },
	{ .compatible = "pcdev-C1x", .data = (void *)PCDEVC1X },
	{ .compatible = "pcdev-D1x", .data = (void *)PCDEVD1X },
	{ }
};

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
	dev_info(&pdev->dev, "device is removed \n");
	return 0;
}

struct pcdev_platform_data *pcdev_get_platdata_from_dt(struct device *dev)
{
	struct device_node *dev_node = dev->of_node;
	struct pcdev_platform_data *pdata;

	if(!dev_node)
		/* probe did not find device tree node */
		return NULL;
	
	/* Allocate memory for platform data */
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if(!pdata) {
		dev_info(dev, "Cannot allocate memory\n");
		return ERR_PTR(-ENOMEM);
	}

	if (of_property_read_string(dev_node, "org,device-serial-num", &pdata->serial_number)) {
		dev_info(dev, "Device serial number property not found\n");
		return ERR_PTR(-EINVAL);
	}

	if(of_property_read_u32(dev_node, "org,size", &pdata->size)) {
		dev_info(dev, "Device size property not found\n");
		return ERR_PTR(-EINVAL);
	}

	if(of_property_read_u32(dev_node, "org,perm", &pdata->perm)) {
		dev_info(dev, "Device permission property not found\n");
		return ERR_PTR(-EINVAL);
	}

	return pdata;
}

/* Gets called when matched platform device is found */
int pcd_platform_driver_probe( struct platform_device *pdev )
{
	int ret = 0;

	struct pcdev_priv_data *dev_data;
	struct pcdev_platform_data *pdata;
	const struct of_device_id *match;
	int driver_data;

	dev_info(&pdev->dev, "Device is detected\n");

	/* of_match_device will always be NULL if the build doesn't support device tree i.e. CONFIG_OF is not set */
	match = of_match_device(of_match_ptr(org_pcdev_dt_match), &pdev->dev);
	if(match) {
		pdata = pcdev_get_platdata_from_dt(&pdev->dev);
		if(IS_ERR(pdata))
			return PTR_ERR(pdata);
		driver_data = (int)(uintptr_t)match->data;
	} else {
		pdata = (struct pcdev_platform_data *) dev_get_platdata(&pdev->dev);		
		driver_data = pdev->id_entry->driver_data;
	}

	if(IS_ERR(pdata))
		return PTR_ERR(pdata);

	/* 2. Dynamically allocate memory for the device private data */
	dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
	if(!dev_data) {
		dev_info(&pdev->dev, "Cannot allocate memory \n");
		ret = -ENOMEM;
		return ret;
	}

	/* Save the device private data pointer in the platform device structure  */
	dev_set_drvdata(&pdev->dev, dev_data);

	dev_data->pdata.size = pdata->size;
	dev_data->pdata.perm = pdata->perm;
	dev_data->pdata.serial_number = pdata->serial_number;
	
	dev_info(&pdev->dev, "Device serial number = %s\n", dev_data->pdata.serial_number);
	dev_info(&pdev->dev, "Device size = %d\n", dev_data->pdata.size);
	dev_info(&pdev->dev, "Device permissions = %d\n", dev_data->pdata.perm);
	dev_info(&pdev->dev, "Device config item 1 = %d\n", pcdev_config[driver_data].config_item1);
	dev_info(&pdev->dev, "Device config item 2 = %d\n", pcdev_config[driver_data].config_item2);

	/* 3. Dynamically allocate memory for the device buffer using size 
	information from the platform data  */
	dev_data->buffer = devm_kzalloc(&pdev->dev, dev_data->pdata.size, GFP_KERNEL);
	if(!dev_data->buffer) {
		dev_info(&pdev->dev, "Cannot allocate memory \n");
		ret = -ENOMEM;
		return ret;
	}

	/* 4. Get the device number */
	dev_data->dev_num = pcdrv_data.device_num_base + pcdrv_data.total_devs;

	/* 5. Do cdev init and cdev add */
	cdev_init(&dev_data->cdev, &pcd_fops);
	dev_data->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
	if(ret < 0) {
		dev_err(&pdev->dev, "cdev add failed \n");
		return ret;
	}

	/* 6. Create device file for the detected platform device */
	pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, &pdev->dev, dev_data->dev_num, NULL, "pcdev-%d", pcdrv_data.total_devs);
	if (IS_ERR(pcdrv_data.device_pcd))
	{
        	dev_err(&pdev->dev, "device creation failed!\n");
        	ret = PTR_ERR(pcdrv_data.device_pcd);
			cdev_del(&dev_data->cdev);
			return ret;
 	}	

	pcdrv_data.total_devs++;

	dev_info(&pdev->dev, "Probe was successful \n");

	return ret;	
}

struct platform_driver pcd_platform_driver = 
{
	.probe = pcd_platform_driver_probe,
	.remove = pcd_platform_driver_remove,
	.id_table = pcdev_ids,
	.driver = {
		.name = "pseudo-char-device",
		.of_match_table = of_match_ptr(org_pcdev_dt_match),
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
