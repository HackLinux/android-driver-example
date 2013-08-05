 #include <linux/init.h>
 #include <linux/module.h>
 #include <linux/types.h>
 #include <linux/fs.h>
 #include <linux/proc_fs.h>
 #include <linux/device.h>
 #include <asm/uaccess.h>
 
#include <linux/cdev.h>
#include <linux/semaphore.h>
 #define EXAMPLE_DEVICE_NODE_NAME  "example"
#define EXAMPLE_DEVICE_FILE_NAME  "example"
#define EXAMPLE_DEVICE_PROC_NAME  "example"
#define EXAMPLE_DEVICE_CLASS_NAME "example"
#define EXAMPLE_MAJOR 0

struct example_dev {
    struct semaphore sem;
    struct cdev cdev;
    int val;
};
 static int example_major = EXAMPLE_MAJOR;
 static int example_minor = 0;
 
 static struct class* example_class = NULL;
 static struct example_dev* example_dev = NULL;
 
 module_param(example_major, int, S_IRUGO);
 module_param(example_minor, int, S_IRUGO);
 
 static int example_open(struct inode* inode, struct file* filp)
 {
     struct example_dev* dev;
 
     /*
      * ȡ���豸�ṹ��example_dev��������filp˽���������У��Է�����������ʹ�á�
      */
     dev = container_of(inode->i_cdev, struct example_dev, cdev);
    filp->private_data = dev;

    return 0;
}

static int example_release(struct inode* inode, struct file* filp)
{
    return 0;
}

static ssize_t example_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos)
{
    struct example_dev* dev = filp->private_data;
    ssize_t retval = 0;

    /*
     * ����
     */
    if(down_interruptible(&(dev->sem)))
        return -ERESTARTSYS;

    if(count < sizeof(dev->val))
        goto out;

    /*
     * ���Ĵ�����ֵ���û��ռ䡣
     */
    if(copy_to_user(buf, &(dev->val), sizeof(dev->val)))
    {
        retval = -EFAULT;
        goto out;
    }

    retval = sizeof(dev->val);

out:
    /*
     * ����
     */
    up(&(dev->sem));
    return retval;
}
 
 static ssize_t example_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
 {
     struct example_dev* dev = filp->private_data;
     ssize_t retval = 0;
 
     /*
      * ����
      */
     if(down_interruptible(&(dev->sem)))
         return -ERESTARTSYS;
 
     if(count != sizeof(dev->val))
         goto out;
 
     /*
      * ���û��ռ��ȡ�����Ĵ�����ֵ��
      */
     if(copy_from_user(&(dev->val), buf, count))
     {
         retval = -EFAULT;
         goto out;
     }
 
     retval = sizeof(dev->val);
 
 out:
     /*
      * ����
      */
    up(&(dev->sem));
    return retval;
}

/*
 * �豸����������
 */
static struct file_operations example_fops =
{
    .owner = THIS_MODULE,
    .open = example_open,
    .release = example_release,
    .read = example_read,
    .write = example_write,
};


/*
 * ��ͬ��״̬�¶�ȡ�Ĵ�����ֵ��
 */
static ssize_t __example_get_val(struct example_dev* dev, char* buf)
{
    int val = 0;

    if(down_interruptible(&(dev->sem)))
        return -ERESTARTSYS;

    val = dev->val;
    up(&(dev->sem));

    return snprintf(buf, 30, "%d\n", val);
}

/*
 * ��ͬ��״̬�����üĴ�����ֵ��
 */
static ssize_t __example_set_val(struct example_dev* dev, const char* buf, size_t count)
{
    int val = 0;

    val = (int)simple_strtol(buf, NULL, 10);

    if(down_interruptible(&(dev->sem)))
        return -ERESTARTSYS;

    dev->val = val;
    up(&(dev->sem));

    return count;
}

/*
 * �������ļ��Ķ�ȡ����������
 */
 static ssize_t example_val_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    struct example_dev* hdev = (struct example_dev*)dev_get_drvdata(dev);

    return __example_get_val(hdev, buf);
}

/*
 * �������ļ���д����������
 */
static  ssize_t example_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    struct example_dev* hdev = (struct example_dev*)dev_get_drvdata(dev);

    return __example_set_val(hdev, buf, count);
}

/*
 * DEVICE_ATTR��չ�������ɵ���dev_attr_val��
 * ָ��������Ϊ"val������Ӧ�Ķ�д�����ֱ���example_val_show��example_val_store��
 */
static DEVICE_ATTR(val, S_IRUGO | S_IWUSR, example_val_show, example_val_store);

/*
 * /proc�ڵ�Ķ�����������
 */
static ssize_t example_proc_read(char* page, char** start, off_t off, int count, int* eof, void* data)
{
    if(off > 0)
    {
        *eof = 1;
        return 0;
    }

    return __example_get_val(example_dev, page);
}

/*
 * /proc�ڵ��д����������
*/
static ssize_t example_proc_write(struct file* filp, const char __user *buff, unsigned long len, void* data)
{
    int err = 0;
    char* page = NULL;

    if(len > PAGE_SIZE)
    {
        printk(KERN_ALERT"The buff is too large: %lu.\n", len);
        return -EFAULT;
    }

   page = (char*)__get_free_page(GFP_KERNEL);
    if(!page)
    {
        printk(KERN_ALERT"Failed to alloc page.\n");
       return -ENOMEM;
    }

    if(copy_from_user(page, buff, len))
    {
        printk(KERN_ALERT"Failed to copy buff from user.\n");
        err = -EFAULT;
        goto out;
    }

    err = __example_set_val(example_dev, page, len);

out:
    free_page((unsigned long)page);
    return err;
}

/*
 * ����/proc�ڵ�
 */
static void example_create_proc(void)
{
    struct proc_dir_entry* entry;

    entry = create_proc_entry(EXAMPLE_DEVICE_PROC_NAME, 0, NULL);
    if(entry)
    {
        //entry->owner = THIS_MODULE;
        entry->read_proc = example_proc_read;
        entry->write_proc = example_proc_write;
    }
}

/*
 * ɾ��/proc�ڵ�
 */
static void example_remove_proc(void)
{
    remove_proc_entry(EXAMPLE_DEVICE_PROC_NAME, NULL);
}

/*
 * ��ʼ���豸�ṹ��example_dev��
 */
static int  __example_setup_dev(struct example_dev* dev)
{
    int retval;

    /*
     * ȡ���豸���
     */
    dev_t devno = MKDEV(example_major, example_minor);

    /*
     * ���豸�ṹ���ڴ�ռ��ʼ��Ϊ0��
     */
    memset(dev, 0, sizeof(struct example_dev));

    /*
     * ��ʼ���豸�ṹ���cdev��Ա��ָ��owner�Ͳ�����������
     */
    cdev_init(&(dev->cdev), &example_fops);
   dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &example_fops;
   /*
    * ����cdev_add��֪ͨ�ں˸��ַ��豸�Ĵ��ڡ�
     */
   retval = cdev_add(&(dev->cdev),devno, 1);
    if(retval)
   {
        return retval;
    }

    /*
     * ��ʼ���ź���
     */
    init_MUTEX(&(dev->sem));

    /*
     *���Ĵ���valֵ��ʼ��Ϊ0
     */
    dev->val = 0;

    return 0;
}

/*
 * ģ���ʼ��������
 */
static int __init example_init(void)
{
    int retval = -1;
    dev_t dev = 0;
    struct device* device = NULL;

    printk(KERN_ALERT"Initializing example device.\n");

    /*
     * ����û�ָ�������豸�ţ���example_major��Ϊ0�������
     * register_chrdev_region����ָ�����豸��š�
     * ����û�û��ָ�����豸�ţ���example_majorΪ0�������
     * alloc_chrdev_region��̬�����豸��š�
     */
    if (example_major) {
        dev = MKDEV(example_major, example_minor);
       retval = register_chrdev_region(dev, 1, EXAMPLE_DEVICE_NODE_NAME);
    } else {
        retval = alloc_chrdev_region(&dev, example_minor, 1, EXAMPLE_DEVICE_NODE_NAME);
    }
    if (retval < 0) {
        printk(KERN_WARNING "can't get example_major %d\n", example_major);
        goto fail;
    }

    /*
     * ȡ�����豸�źʹ��豸��
     */
    example_major = MAJOR(dev);
    example_minor = MINOR(dev);

    /*
     * Ϊ�豸�ṹ��example_dev��̬�����ڴ�ռ䡣
     */
   example_dev = kmalloc(sizeof(struct example_dev), GFP_KERNEL);
    if(!example_dev)
    {
        retval = -ENOMEM;
        printk(KERN_ALERT"Failed to alloc example_dev.\n");
        goto unregister;
    }

    /*
     * ����__example_setup_dev������example_dev�ṹ����г�ʼ����
     */
    retval = __example_setup_dev(example_dev);
    if(retval)
    {
        printk(KERN_ALERT"Failed to setup dev: %d.\n", retval);
        goto cleanup;
    }

    /*
     * ������example��class_create����ִ�гɹ�����/sys/classĿ¼��
     * �ͻ����һ����Ϊexample��Ŀ¼��
     */
    example_class = class_create(THIS_MODULE, EXAMPLE_DEVICE_CLASS_NAME);
    if(IS_ERR(example_class))
    {
        retval = PTR_ERR(example_class);
        printk(KERN_ALERT"Failed to create example class.\n");
        goto destroy_cdev;
    }

    /*
     * �����豸��device_create����ִ�гɹ��󣬻�����/dev/example�ļ�
     * ��/sys/class/example/exampleĿ¼������ļ���
     * ע��device��������struct device������һ���豸��
     */
    device = device_create(example_class, NULL, dev, "%s", EXAMPLE_DEVICE_FILE_NAME);
    if(IS_ERR(device))
    {
        retval = PTR_ERR(device);
        printk(KERN_ALERT"Failed to create example device.");
        goto destroy_class;
    }

    /*
     * ���������ļ�,��Ӧ�����Բ���������dev_attr_valָ����
     */
    retval = device_create_file(device, &dev_attr_val);
    if(retval < 0)
   {
        printk(KERN_ALERT"Failed to create attribute val.");
        goto destroy_device;
    }

    /*
    * ��example_dev�������豸˽���������С�
     */
    dev_set_drvdata(device, example_dev);

    /*
     * ����proc�ڵ㡣
     */
    example_create_proc();

    printk(KERN_ALERT"Succedded to initialize example device.\n");
    return 0;

destroy_device:
    device_destroy(example_class, dev);

destroy_class:
    class_destroy(example_class);

destroy_cdev:
    cdev_del(&(example_dev->cdev));
cleanup:
    kfree(example_dev);

unregister:
    unregister_chrdev_region(MKDEV(example_major, example_minor), 1);

fail:
    return retval;
}
/*
 * ģ����������
 */
static void __exit example_exit(void)
{
    dev_t dev = MKDEV(example_major, example_minor);

    printk(KERN_ALERT"Destroy example device.\n");

    example_remove_proc();

    if(example_class)
    {
        device_destroy(example_class, MKDEV(example_major, example_minor));
        class_destroy(example_class);
    }

    if(example_dev)
    {
        cdev_del(&(example_dev->cdev));
        kfree(example_dev);
    }

    unregister_chrdev_region(dev, 1);
}

MODULE_LICENSE("GPL");

module_init(example_init);
module_exit(example_exit);
