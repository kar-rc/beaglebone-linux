#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>

#define MAX_DEV_SUPPORT 4

#define DEV0_MEM_SIZE 1024
#define DEV1_MEM_SIZE 512
#define DEV2_MEM_SIZE 2048
#define DEV3_MEM_SIZE 512

#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt, __func__

/* pseudo device's memory */
char device0_buffer[DEV0_MEM_SIZE];
char device1_buffer[DEV1_MEM_SIZE];
char device2_buffer[DEV2_MEM_SIZE];
char device3_buffer[DEV3_MEM_SIZE];

/* Device Private Data Structure */
struct pcdev_priv_data
{
	char * buffer;
	unsigned size;
	const char *serial_num;
	int perm;
	struct cdev cdev;
};

/* Driver Private Data Structure */
struct pcdrv_priv_data
{
	int total_devs;
	/* Device number  */
	dev_t device_number;
	struct pcdev_priv_data pcdev_data[MAX_DEV_SUPPORT]; 
	struct class * class_pcd;
	struct device * device_pcd;
};


#define RDONLY 0x01
#define WRONLY 0x10
#define RDWR   0x11

struct pcdrv_priv_data pcdrv = 
{
	.total_devs = MAX_DEV_SUPPORT,
	.pcdev_data = {
		[0] = {
			.buffer = device0_buffer,
			.size = DEV0_MEM_SIZE,
			.serial_num = "PCDDEV0000",
			.perm = RDONLY
		},
		[1] = {
			.buffer = device1_buffer,
			.size = DEV1_MEM_SIZE,
			.serial_num = "PCDDEV0001",
			.perm = WRONLY
		},
		[2] = {
			.buffer = device2_buffer,
			.size = DEV2_MEM_SIZE,
			.serial_num = "PCDDEV0002",
			.perm = RDWR
		},
		[3] = {
			.buffer = device3_buffer,
			.size = DEV3_MEM_SIZE,
			.serial_num = "PCDDEV0003",
			.perm = RDWR
		}
	}
};

loff_t pcd_lseek (struct file * filp, loff_t off, int whence)
{
	struct pcdev_priv_data * pcdev_d = (struct pcdev_priv_data *)filp->private_data;	
	int max_size = pcdev_d->size;

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
	int max_size = pcdev_d->size;

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
	int max_size = pcdev_d->size;
	
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
	int ret;
	int minor_n;
	struct pcdev_priv_data * pcdev_d;
	
	/* Find out on which device file open was attempted by the user space */
	minor_n = MINOR(inode->i_rdev);
	pr_info("minor device number = %d\n", minor_n);
	
	/* Get device's private data structure */
	pcdev_d = container_of(inode->i_cdev, struct pcdev_priv_data, cdev);	

	/* Store in filp private data to supply to other methods of the driver */
	filp->private_data = pcdev_d;	

	/* Check permissions */
	ret = check_permission(pcdev_d->perm, filp->f_mode);

	(!ret)?pr_info("open was successful \n"):pr_info("open was unsuccessful \n");
	
	return ret;
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

static int __init pcd_driver_init(void)
{
	int ret;
	int i = 0;
	/* 1. Dynamically allocate a device number */
	ret = alloc_chrdev_region(&pcdrv.device_number, 0, MAX_DEV_SUPPORT, "pcd_devices");
	if (ret < 0)
	{
		pr_err("allocate char device region failed!\n");
		goto out;
	}
	
	/* 4. create device class under /sys/class/ */
	pcdrv.class_pcd = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(pcdrv.class_pcd))
	{
		pr_err("class creation failed!\n");
		ret = PTR_ERR(pcdrv.class_pcd);
		goto unreg_chrdev;
	}
	
	while (i < MAX_DEV_SUPPORT) {
		pr_info("device number <major>:<minor> = %d:%d\n", MAJOR( pcdrv.device_number + i ), MINOR( pcdrv.device_number + i ));
	
		/* Initialize the cdev structure with fops */
		cdev_init(&pcdrv.pcdev_data[i].cdev, &pcd_fops);
	
		/* Registration of the cdev structure with VFS */
        	pcdrv.pcdev_data[i].cdev.owner = THIS_MODULE;
		ret = cdev_add(&pcdrv.pcdev_data[i].cdev, pcdrv.device_number + i, 1);
		if(ret < 0)
		{
			pr_err("char device add failed!\n");
			goto cdev_del;
		}

		/* Populate the sysfs with device information  */
		pcdrv.device_pcd = device_create(pcdrv.class_pcd, NULL, pcdrv.device_number + i, NULL, "pcdev-%d",i);
        	if (IS_ERR(pcdrv.device_pcd))
        	{
                	pr_err("device creation failed!\n");
                	ret = PTR_ERR(pcdrv.device_pcd);
                	goto class_del;
       	 	}

		pr_info("module init was succesful!\n");
	
		i++;
	}
	return 0;

cdev_del:
class_del:
	while (i >= 0) {
		device_destroy(pcdrv.class_pcd, pcdrv.device_number + i);
		cdev_del(&pcdrv.pcdev_data[i].cdev);
		i--;
	}
	class_destroy(pcdrv.class_pcd);

unreg_chrdev:
	unregister_chrdev_region(pcdrv.device_number, MAX_DEV_SUPPORT);

out:
	pr_info("Module Insetion failed!\n");
	return ret;
}


static void __exit pcd_driver_cleanup(void)
{
	int i = 0;
	while (i < MAX_DEV_SUPPORT) {
		device_destroy(pcdrv.class_pcd, pcdrv.device_number + i);
		cdev_del(&pcdrv.pcdev_data[i].cdev);
		i++;
	}

	class_destroy(pcdrv.class_pcd);

	unregister_chrdev_region(pcdrv.device_number, MAX_DEV_SUPPORT);

	pr_info("Module unloaded\n");
}


module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ME");
MODULE_DESCRIPTION("Pseudo Character Driver supporting N devices");

