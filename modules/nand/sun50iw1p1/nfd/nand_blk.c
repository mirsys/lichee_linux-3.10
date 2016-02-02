#include <linux/compat.h>
#include <asm/uaccess.h>
#include "nand_blk.h"
#include "nand_dev.h"

/*****************************************************************************/

#define REMAIN_SPACE 0
#define PART_FREE 0x55
#define PART_DUMMY 0xff
#define PART_READONLY 0x85
#define PART_WRITEONLY 0x86
#define PART_NO_ACCESS 0x87

#define TIMEOUT                 300         // ms
#define NAND_CACHE_RW

#define NAND_IO_RESPONSE_TEST
static unsigned int dragonboard_test_flag = 0;
//#define NAND_IO_RESPONSE_TEST
struct secblc_op_t{
	int item;
	unsigned char *buf;
	unsigned int len ;
};


struct burn_param_t{
    void* buffer;
  long length;
};

#ifdef CONFIG_COMPAT

struct secblc_op_t32{
	int item;
	compat_uptr_t buf;
	unsigned int len ;
};


struct burn_param_t32{
    compat_uptr_t buffer;
  	compat_size_t length;
};

struct hd_geometry32 {
      unsigned char heads;
      unsigned char sectors;
      unsigned short cylinders;
      compat_size_t start;
};

#endif

/*****************************************************************************/
extern int get_nand_secure_storage_max_item(void);
extern int nand_secure_storage_read(int item,unsigned char *buf,unsigned int len);
extern int nand_secure_storage_write(int item,unsigned char *buf,unsigned int len);

extern int NAND_BurnBoot0(uint length, void *buf);
extern int NAND_BurnBoot1(uint length, void *buf);

extern struct _nand_info* p_nand_info;

//for Int
//extern void NAND_ClearRbInt(void);
//extern void NAND_ClearDMAInt(void);
//extern void NAND_Interrupt(__u32 nand_index);

extern  int add_nand(struct nand_blk_ops *tr, struct _nand_phy_partition* phy_partition);
extern  int add_nand_for_dragonboard_test(struct nand_blk_ops *tr);
extern  int remove_nand(struct nand_blk_ops *tr);
extern  int nand_flush(struct nand_blk_dev *dev);
extern struct _nand_phy_partition* get_head_phy_partition_from_nand_info(struct _nand_info*nand_info);
extern struct _nand_phy_partition* get_next_phy_partition(struct _nand_phy_partition* phy_partition);

/*****************************************************************************/

DEFINE_SEMAPHORE(nand_mutex);

unsigned char volatile IS_IDLE = 1;
static int nand_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
static int nand_compat_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg);
#endif
long max_r_io_response = 1;
long max_w_io_response = 1;

int debug_data = 0;

struct timeval tpstart,tpend;
long timeuse;


///* print flags by name */
//const char *rq_flag_bit_names[] = {
//	"REQ_WRITE",		         /* not set, read. set, write */
//	"REQ_FAILFAST_DEV",	         /* no driver retries of device errors */
//	"REQ_FAILFAST_TRANSPORT",    /* no driver retries of transport errors */
//	"REQ_FAILFAST_DRIVER",	     /* no driver retries of driver errors */
//	"REQ_SYNC",		             /* request is sync (sync write or read) */
//	"REQ_META",		             /* metadata io request */
//	"REQ_DISCARD",		         /* request to discard sectors */
//	"REQ_NOIDLE",		         /* don't anticipate more IO after this one */
//	"REQ_RAHEAD",		         /* read ahead, can fail anytime */ /* bio only flags */
//	"REQ_THROTTLED",	         /* This bio has already been subjected to * throttling rules. Don't do it again. */
//	"REQ_SORTED",		         /* elevator knows about this request */
//	"REQ_SOFTBARRIER",	         /* may not be passed by ioscheduler */
//	"REQ_FUA",		             /* forced unit access */
//	"REQ_NOMERGE",		         /* don't touch this for merging */
//	"REQ_STARTED",		         /* drive already may have started this one */
//	"REQ_DONTPREP",		         /* don't call prep for this one */
//	"REQ_QUEUED",		         /* uses queueing */
//	"REQ_ELVPRIV",		         /* elevator private data attached */
//	"REQ_FAILED",		         /* set if the request failed */
//	"REQ_QUIET",		         /* don't worry about errors */
//	"REQ_PREEMPT",		         /* set for "ide_preempt" requests */
//	"REQ_ALLOCED",		         /* request came from our alloc pool */
//	"REQ_COPY_USER",	         /* contains copies of user pages */
//	"REQ_FLUSH",		         /* request for cache flush */
//	"REQ_FLUSH_SEQ",	         /* request for flush sequence */
//	"REQ_IO_STAT",		         /* account I/O stat */
//	"REQ_MIXED_MERGE",	         /* merge of different types, fail separately */
//	"REQ_SECURE",		         /* secure discard (used with __REQ_DISCARD) */
//	"REQ_NR_BITS",		        /* stops here */
//};
//void print_rq_flags(int flags)
//{
//	int i;
//	uint32_t j;
//	j = 1;
//	printk("rq:");
//	for (i = 0; i < 32; i++) {
//		if (flags & j)
//			printk("%s ", rq_flag_bit_names[i]);
//		j = j << 1;
//	}
//	printk("\n");
//}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void start_time(int data)
{
#ifdef NAND_IO_RESPONSE_TEST

    if(debug_data != data)
        return;

    do_gettimeofday(&tpstart);

#endif
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int end_time(int data,int time,int par)
{
#ifdef NAND_IO_RESPONSE_TEST

    if(debug_data != data)
        return -1;

    do_gettimeofday(&tpend);
    timeuse = 1000*(tpend.tv_sec-tpstart.tv_sec)*1000+(tpend.tv_usec-tpstart.tv_usec);
    if(timeuse > time)
    {
        printk("%ld %d\n", timeuse, par);
        return 1;
    }

#endif
    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int do_blktrans_request(struct nand_blk_ops *tr,struct nand_blk_dev *dev,struct request *req)
{
    int ret = 0;
    unsigned int block, nsect;
    char *buf;

    struct _nand_dev* nand_dev;
    nand_dev = (struct _nand_dev*)dev->priv;

    block = blk_rq_pos(req) << 9 >> tr->blkshift;
    nsect = blk_rq_cur_bytes(req) >> tr->blkshift;

    buf = req->buffer;

    if (req->cmd_type != REQ_TYPE_FS)
    {
        nand_dbg_err(KERN_NOTICE "not type fs\n");
        return -EIO;
    }

    if (blk_rq_pos(req) + blk_rq_cur_sectors(req) > get_capacity(req->rq_disk))
    {
        nand_dbg_err(KERN_NOTICE "over capacity\n");
        return -EIO;
    }

    if (req->cmd_flags & REQ_DISCARD)
    {
        //nand_dbg_err("REQ_DISCARD block: %d nsect: %d\n", block,nsect);
        //rq_flush_dcache_pages(req);
        nand_dev->discard(nand_dev,block,nsect);
        goto request_exit;
    }

    switch(rq_data_dir(req)) 
    {
        case READ:
            if(debug_data == 10)
            {
                nand_dbg_err("read_addr: %d nsect: %d buf: %p\n", block,nsect,buf);
            }

            //start_time(1);

            nand_dev->read_data(nand_dev,block,nsect,buf);

            //end_time(1,100000,nsect);

            rq_flush_dcache_pages(req);
            ret = 0;
            goto request_exit;

        case WRITE:
            rq_flush_dcache_pages(req);

            if(debug_data == 10)
            {
                nand_dbg_err("write_addr: %d nsect: %d buf: %p\n", block,nsect,buf);
            }

            //start_time(2);
            nand_dev->write_data(nand_dev,block,nsect,buf);
            //end_time(2,100000,nsect);

            ret = 0;

            goto request_exit;
        default:
            nand_dbg_err(KERN_NOTICE "Unknown request %u\n", rq_data_dir(req));
            ret = -EIO;
            goto request_exit;
    }

request_exit:
    return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int mtd_blktrans_thread(void *arg)
{
    struct nand_blk_dev *dev = arg;
    struct request_queue *rq = dev->rq;
    struct request *req = NULL;
    int background_done = 0;

    spin_lock_irq(rq->queue_lock);

    while (!kthread_should_stop()) {
        int res;

        dev->bg_stop = false;
        if (!req && !(req = blk_fetch_request(rq))) {

//          if (tr->background && !background_done) {
//              spin_unlock_irq(rq->queue_lock);
//              mutex_lock(&dev->lock);
//              tr->background(dev);
//              mutex_unlock(&dev->lock);
//              spin_lock_irq(rq->queue_lock);
//              /*
//               * Do background processing just once per idle
//               * period.
//               */
//              background_done = !dev->bg_stop;
//              continue;
//          }

            set_current_state(TASK_INTERRUPTIBLE);

            if (kthread_should_stop())
                set_current_state(TASK_RUNNING);

            spin_unlock_irq(rq->queue_lock);
            dev->rq_null++;
            schedule();
            spin_lock_irq(rq->queue_lock);
            continue;
        }

        spin_unlock_irq(rq->queue_lock);
        dev->rq_null = 0;
        mutex_lock(&dev->lock);
        res = do_blktrans_request(dev->nandr, dev, req);
        mutex_unlock(&dev->lock);

        spin_lock_irq(rq->queue_lock);

        if (!__blk_end_request_cur(req, res))
            req = NULL;

        background_done = 0;
    }

    if (req)
        __blk_end_request_all(req, -EIO);

    spin_unlock_irq(rq->queue_lock);

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void mtd_blktrans_request(struct request_queue *rq)
{
    struct nand_blk_dev *dev;
    struct request *req = NULL;

    dev = rq->queuedata;

    if (!dev)
        while ((req = blk_fetch_request(rq)) != NULL)
            __blk_end_request_all(req, -ENODEV);
    else {
        dev->bg_stop = true;
        wake_up_process(dev->thread);
    }
}

static void null_for_dragonboard(struct request_queue *rq)
{
	return ;
}



/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int nand_open(struct block_device *bdev, fmode_t mode)
{
    struct nand_blk_dev *dev;
    struct nand_blk_ops *nandr;
    int ret = -ENODEV;

    dev = bdev->bd_disk->private_data;
    nandr = dev->nandr;

    if (!try_module_get(nandr->owner))
        goto out;

    ret = 0;
    if (nandr->open && (ret = nandr->open(dev))) {
        out:
        module_put(nandr->owner);
    }
    return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void nand_release(struct gendisk *disk, fmode_t mode)
{
    struct nand_blk_dev *dev;
    struct nand_blk_ops *nandr;

    int ret = 0;

    dev = disk->private_data;
    nandr = dev->nandr;
    if (nandr->release)
        ret = nandr->release(dev);

    if (!ret) {
        module_put(nandr->owner);
    }

//    return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
#define DISABLE_WRITE           _IO('V',0)
#define ENABLE_WRITE            _IO('V',1)
#define DISABLE_READ            _IO('V',2)
#define ENABLE_READ             _IO('V',3)
#define DRAGON_BOARD_TEST    _IO('V',55)  
#define BLKBURNBOOT0            _IO('v',127)
#define BLKBURNBOOT1            _IO('v',128)
#define SECBLK_READ				_IO('V',20)
#define SECBLK_WRITE			_IO('V',21)
#define SECBLK_IOCTL			_IO('V',22)

static int nand_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    struct nand_blk_dev *dev = bdev->bd_disk->private_data;
    struct nand_blk_ops *nandr = dev->nandr;
    struct burn_param_t *burn_param;
	struct secblc_op_t *sec_op;
    int ret=0;
	unsigned char *buf_secure = NULL;

    burn_param = (struct burn_param_t *)arg;

    switch (cmd) {
    case BLKFLSBUF:
        //nand_dbg_err("BLKFLSBUF called!\n");
        if (nandr->flush)
            return nandr->flush(dev);
        // The core code did the work, we had nothing to do.
        return 0;

    case HDIO_GETGEO:
        if (nandr->getgeo) {
            struct hd_geometry g;
            int ret;

            memset(&g, 0, sizeof(g));
            ret = nandr->getgeo(dev, &g);
            if (ret)
                return ret;
            nand_dbg_err("HDIO_GETGEO called!\n");
            g.start = get_start_sect(bdev);
            if (copy_to_user((void __user *)arg, &g, sizeof(g)))
                return -EFAULT;

            return 0;
        }
        return 0;
    case ENABLE_WRITE:
        nand_dbg_err("enable write!\n");
        dev->disable_access = 0;
        dev->readonly = 0;
        set_disk_ro(dev->disk, 0);
        return 0;

    case DISABLE_WRITE:
        nand_dbg_err("disable write!\n");
        dev->readonly = 1;
        set_disk_ro(dev->disk, 1);
        return 0;

    case ENABLE_READ:
        nand_dbg_err("enable read!\n");
        dev->disable_access = 0;
        dev->writeonly = 0;
        return 0;

    case DISABLE_READ:
        nand_dbg_err("disable read!\n");
        dev->writeonly = 1;
        return 0;

    case BLKBURNBOOT0:
		nand_dbg_err("start BLKBURNBOOT0...\n");
		down(&nandr->nand_ops_mutex);
		IS_IDLE = 0;
		ret = NAND_BurnBoot0(burn_param->length, burn_param->buffer);
		up(&(nandr->nand_ops_mutex));
		nand_dbg_err("do BLKBURNBOOT0!\n");
		IS_IDLE = 1;
		return ret;

    case BLKBURNBOOT1:

		nand_dbg_err("start BLKBURNBOOT1...\n");
		down(&nandr->nand_ops_mutex);
		IS_IDLE = 0;
		ret = NAND_BurnBoot1(burn_param->length, burn_param->buffer);
		up(&(nandr->nand_ops_mutex));
		nand_dbg_err("do BLKBURNBOOT1!\n");
		IS_IDLE = 1;
        return ret;

	case SECBLK_READ:

		nand_dbg_err("start secure read ...\n");
		down(&nandr->nand_ops_mutex);
		IS_IDLE = 0;
		sec_op = (struct secblc_op_t *)arg ;
		buf_secure =(unsigned char *)kmalloc(sec_op->len,GFP_KERNEL);
		if(buf_secure == NULL)
		{
	        nand_dbg_err("buf_secure malloc fail!\n");
	        return -1;
		}
		ret = nand_secure_storage_read(sec_op->item, buf_secure, sec_op->len);
		if (copy_to_user(sec_op->buf, buf_secure, sec_op->len))
			ret = -EFAULT;
		kfree(buf_secure);
		up(&(nandr->nand_ops_mutex));
		IS_IDLE = 1;
		return ret;

	case SECBLK_WRITE:

		nand_dbg_err("start secure write ...\n");
		down(&nandr->nand_ops_mutex);
		IS_IDLE = 0;
		sec_op = (struct secblc_op_t *)arg ;
		buf_secure =(unsigned char *)kmalloc(sec_op->len,GFP_KERNEL);
		if(buf_secure == NULL)
		{
	        nand_dbg_err("buf_secure malloc fail!\n");
	        return -1;
		}
		if (copy_from_user(buf_secure, (const void*)sec_op->buf, sec_op->len))
			ret = -EFAULT;
		ret = nand_secure_storage_write(sec_op->item, buf_secure, sec_op->len);
		kfree(buf_secure);
		up(&(nandr->nand_ops_mutex));
		IS_IDLE = 1;
		return ret;

	case SECBLK_IOCTL:

		nand_dbg_err("start secure get item...\n");
		down(&nandr->nand_ops_mutex);
		IS_IDLE = 0;
		ret = get_nand_secure_storage_max_item();
		up(&(nandr->nand_ops_mutex));
		IS_IDLE = 1;
		return ret;
		
	case DRAGON_BOARD_TEST:
	
		nand_dbg_err("start dragonborad test...\n");
		down(&(nandr->nand_ops_mutex));
		IS_IDLE = 0;
		ret = NAND_DragonboardTest();
		up(&(nandr->nand_ops_mutex));
		IS_IDLE = 1;
		return ret;
		
    default:
		nand_dbg_err("unknow cmd 0x%x!\n",cmd);
        return -ENOTTY;
    }
}

int nand_burnboot0_compat(struct block_device *bdev, fmode_t mode, unsigned int cmd, 
struct burn_param_t32 __user *arg)
{
	struct burn_param_t32 burn_param32;
	struct burn_param_t __user *burn_param;
	int ret;

	if (copy_from_user(&burn_param32, arg, sizeof(burn_param32)))
		return -EFAULT;

	burn_param = compat_alloc_user_space(sizeof(*burn_param));
	if (!access_ok(VERIFY_WRITE, burn_param, sizeof(*burn_param)))
		return -EFAULT;

	if (put_user(compat_ptr(burn_param32.buffer),&burn_param->buffer) ||
			put_user(burn_param32.length,&burn_param->length))
		return -EFAULT;

	ret = nand_ioctl(bdev, mode, cmd, (unsigned long) burn_param);
	if (ret < 0)
		return ret;

	return 0;
}

int nand_burnboot1_compat(struct block_device *bdev, fmode_t mode, unsigned int cmd, 
struct burn_param_t32 __user *arg)
{
	struct burn_param_t32 burn_param32;
	struct burn_param_t __user *burn_param;
	int ret;

	if (copy_from_user(&burn_param32, arg, sizeof(burn_param32)))
		return -EFAULT;

	burn_param = compat_alloc_user_space(sizeof(*burn_param));
	if (!access_ok(VERIFY_WRITE, burn_param, sizeof(*burn_param)))
		return -EFAULT;

	if (put_user(compat_ptr(burn_param32.buffer),&burn_param->buffer) ||
			put_user(burn_param32.length,&burn_param->length))
		return -EFAULT;

	ret = nand_ioctl(bdev, mode, cmd, (unsigned long) burn_param);
	if (ret < 0)
		return ret;

	return 0;
}

int nand_drangonboard_compat(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long __user *arg)
{
	int ret;

	ret = nand_ioctl(bdev, mode, cmd, (unsigned long) arg);
	if (ret < 0)
		return ret;

	return 0;
}

int nand_securestorage_compat(struct block_device *bdev, fmode_t mode, unsigned int cmd, 
struct secblc_op_t32 __user *arg)
{
	struct secblc_op_t32 secblc_op32;
	struct secblc_op_t __user *secblc_op;
	int ret;

	if (copy_from_user(&secblc_op32, arg, sizeof(secblc_op32)))
		return -EFAULT;

	secblc_op = compat_alloc_user_space(sizeof(*secblc_op));
	if (!access_ok(VERIFY_WRITE, secblc_op, sizeof(*secblc_op)))
		return -EFAULT;

	if (put_user(secblc_op32.item,&secblc_op->item) || 
		put_user(compat_ptr(secblc_op32.buf),&secblc_op->buf) ||
			put_user(secblc_op32.len,&secblc_op->len))
		return -EFAULT;

	ret = nand_ioctl(bdev, mode, cmd, (unsigned long) secblc_op);
	if (ret < 0)
		return ret;

	if (copy_in_user(&arg->item,&secblc_op->item,sizeof(arg->item)) ||
		copy_in_user(&arg->len,&secblc_op->len,sizeof(arg->len)))
		return -EFAULT;

	return 0;
}

int nand_getgeo_compat(struct block_device *bdev, fmode_t mode, unsigned int cmd, 
struct hd_geometry32 __user *arg)
{
	struct hd_geometry32 getgeo32;
	struct hd_geometry __user *getgeo;
	int ret;

	if (copy_from_user(&getgeo32, arg, sizeof(getgeo32)))
		return -EFAULT;

	getgeo = compat_alloc_user_space(sizeof(*getgeo));
	if (!access_ok(VERIFY_WRITE, getgeo, sizeof(*getgeo)))
		return -EFAULT;

	if (put_user(getgeo32.heads,&getgeo->heads) || 
		put_user(getgeo32.sectors,&getgeo->sectors) ||
		put_user(getgeo32.cylinders,&getgeo->cylinders) ||
		put_user(getgeo32.start,&getgeo->start))
		return -EFAULT;

	ret = nand_ioctl(bdev, mode, cmd, (unsigned long) getgeo);
	if (ret < 0)
		return ret;

	if (copy_in_user(&arg->heads,&getgeo->heads,sizeof(arg->heads)) ||
		copy_in_user(&arg->sectors,&getgeo->sectors,sizeof(arg->sectors)) ||
		copy_in_user(&arg->cylinders,&getgeo->cylinders,sizeof(arg->cylinders)) ||
		copy_in_user(&arg->start,&getgeo->start,sizeof(arg->start)))
		return -EFAULT;

	return 0;
}

static int nand_compat_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	nand_dbg_err("start nand_compat_ioctl\n");
	switch (cmd) {
		case HDIO_GETGEO:
			return nand_getgeo_compat(bdev, mode, cmd, compat_ptr(arg));
			
		case BLKBURNBOOT0:
			return nand_burnboot0_compat(bdev, mode, cmd, compat_ptr(arg));

		case BLKBURNBOOT1:
			return nand_burnboot1_compat(bdev, mode, cmd, compat_ptr(arg));

		case DRAGON_BOARD_TEST:
			return nand_drangonboard_compat(bdev, mode, cmd, compat_ptr(arg));

		case SECBLK_READ:
		case SECBLK_WRITE:
			return nand_securestorage_compat(bdev, mode, cmd, compat_ptr(arg));

		default:
			return nand_ioctl(bdev, mode, cmd, arg);
	}	
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/

struct block_device_operations nand_blktrans_ops = {
    .owner      = THIS_MODULE,
    .open       = nand_open,
    .release    = nand_release,
    .ioctl      = nand_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nand_compat_ioctl,
#endif

};


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int nand_blk_open(struct nand_blk_dev *dev)
{
    //nand_dbg_err("nand_blk_open!\n");
    //mutex_lock(&dev->lock);
    //nand_dbg_err("nand_open ok!\n");

    //kref_get(&dev->ref);

    //mutex_unlock(&dev->lock);
    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int nand_blk_release(struct nand_blk_dev *dev)
{
    int error = 0;
    struct _nand_dev *nand_dev = (struct _nand_dev *)dev->priv;
	if(dragonboard_test_flag == 0)
	{
    //nand_dbg_err("nand_blk_release!\n");

    //error = nand_dev->flush_sector_write_cache(nand_dev,0);
	    error = nand_dev->flush_write_cache(nand_dev,0xffff);

	    //mutex_lock(&dev->lock);
	    //kref_put(&dev->ref, del_nand_blktrans_dev);
	    //mutex_unlock(&dev->lock);

	    return error;
	}
	else
		return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int del_nand_blktrans_dev(struct nand_blk_dev *dev)
{
//    if (!down_trylock(&nand_mutex)) {
//        up(&nand_mutex);
//        BUG();
//    }
    blk_cleanup_queue(dev->rq);
    kthread_stop(dev->thread);
    list_del(&dev->list);
    dev->disk->queue = NULL;
    del_gendisk(dev->disk);
    put_disk(dev->disk);

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int nand_getgeo(struct nand_blk_dev *dev,  struct hd_geometry *geo)
{
    geo->heads = dev->heads;
    geo->sectors = dev->sectors;
    geo->cylinders = dev->cylinders;

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
struct nand_blk_ops mytr = {
    .name               =  "nand",
    .major              = 93,
    .minorbits          = 3,
    .blksize            = 512,
    .blkshift           = 9,
    .open               = nand_blk_open,
    .release            = nand_blk_release,
    .getgeo             = nand_getgeo,
    .add_dev            = add_nand,
    .add_dev_test     	= add_nand_for_dragonboard_test,
    .remove_dev         = remove_nand,
    .flush              = nand_flush,
    .owner              = THIS_MODULE,
};

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void set_part_mod(char *name,int cmd)
{
    struct file *filp = NULL;
    filp = filp_open(name, O_RDWR, 0);
    filp->f_dentry->d_inode->i_bdev->bd_disk->fops->ioctl(filp->f_dentry->d_inode->i_bdev, 0, cmd, 0);
    filp_close(filp, current->files);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int add_nand_blktrans_dev(struct nand_blk_dev *dev)
{
    struct nand_blk_ops *tr = dev->nandr;
    struct list_head *this;
    struct gendisk *gd;
    unsigned long temp;
    int ret = -ENOMEM;

    int last_devnum = -1;

    dev->cylinders = 1024;
    dev->heads = 16;

    temp = dev->cylinders * dev->heads;
    dev->sectors = ( dev->size) / temp;
    if ((dev->size) % temp) {
        dev->sectors++;
        temp = dev->cylinders * dev->sectors;
        dev->heads = (dev->size)  / temp;

        if ((dev->size)   % temp) {
            dev->heads++;
            temp = dev->heads * dev->sectors;
            dev->cylinders = (dev->size)  / temp;
        }
    }

    if (!down_trylock(&nand_mutex)) {
        up(&nand_mutex);
        BUG();
    }

    list_for_each(this, &tr->devs) {
        struct nand_blk_dev *tmpdev = list_entry(this, struct nand_blk_dev, list);
        if (dev->devnum == -1) {
            /* Use first free number */
            if (tmpdev->devnum != last_devnum+1) {
                /* Found a free devnum. Plug it in here */
                dev->devnum = last_devnum+1;
                list_add_tail(&dev->list, &tmpdev->list);
                goto added;
            }
        } else if (tmpdev->devnum == dev->devnum) {
            /* Required number taken */
            nand_dbg_err("\nerror00\n");
            return -EBUSY;
        } else if (tmpdev->devnum > dev->devnum) {
            /* Required number was free */
            list_add_tail(&dev->list, &tmpdev->list);
            goto added;
        }
        last_devnum = tmpdev->devnum;
    }
    if (dev->devnum == -1)
        dev->devnum = last_devnum+1;

    if ((dev->devnum <<tr->minorbits) > 256) {
        nand_dbg_err("\nerror00000\n");
        return -EBUSY;
    }

    //init_MUTEX(&dev->sem);
    list_add_tail(&dev->list, &tr->devs);

added:
    gd = alloc_disk(1 << tr->minorbits);
    if (!gd) {
        list_del(&dev->list);
        goto error2;
    }

    gd->major = tr->major;
    gd->first_minor = (dev->devnum) << tr->minorbits;
    gd->fops = &nand_blktrans_ops;

    snprintf(gd->disk_name, sizeof(gd->disk_name),"%s%c", tr->name, (tr->minorbits?'a':'0') + dev->devnum);
    /* 2.5 has capacity in units of 512 bytes while still
       having BLOCK_SIZE_BITS set to 10. Just to keep us amused. */
    set_capacity(gd, dev->size);

    gd->private_data = dev;
    dev->disk = gd;
    gd->queue = dev->rq;

//    /*set rw partition*/
//    if(part->type == PART_NO_ACCESS)
//        dev->disable_access = 1;
//
//    if(part->type == PART_READONLY)
//        dev->readonly = 1;
//
//    if(part->type == PART_WRITEONLY)
//        dev->writeonly = 1;

//    if (dev->readonly)
//        set_disk_ro(gd, 1);

    dev->disable_access = 0;
    dev->readonly = 0;
    dev->writeonly = 0;

    mutex_init(&dev->lock);
    //kref_init(&dev->ref);

    // Create the request queue
    spin_lock_init(&dev->queue_lock);
    //dev->rq = blk_init_queue(nand_blktrans_request, &dev->queue_lock);
    dev->rq = blk_init_queue(mtd_blktrans_request, &dev->queue_lock);
    if (!dev->rq){
        goto error3;
    }
#if 0
    ret = elevator_change(dev->rq, "deadline");
    if(ret){
        blk_cleanup_queue(dev->rq);
        return ret;
    }
#endif
    dev->rq->queuedata = dev;
    blk_queue_logical_block_size(dev->rq, tr->blksize);


    queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, dev->rq);
    dev->rq->limits.max_discard_sectors = UINT_MAX;

    gd->queue = dev->rq;

    //Create processing thread kernel_thread
    //dev->thread = kthread_run(nand_blktrans_thread, dev,"%s%d", tr->name,dev->devnum);
    dev->thread = kthread_run(mtd_blktrans_thread, dev,"%s%d", tr->name,dev->devnum);
    if (IS_ERR(dev->thread)) {
        ret = PTR_ERR(dev->thread);
        goto error4;
    }

    add_disk(gd);

    return 0;

error4:
    nand_dbg_err("\nerror4\n");
    blk_cleanup_queue(dev->rq);
error3:
    nand_dbg_err("\nerror3\n");
    put_disk(dev->disk);
error2:
    nand_dbg_err("\nerror2\n");
    list_del(&dev->list);
    return ret;
}

int add_nand_blktrans_dev_for_dragonboard(struct nand_blk_dev *dev)
{
    struct nand_blk_ops *tr = dev->nandr;
    struct gendisk *gd;
    int ret = -ENOMEM;

	gd = alloc_disk(1);
	if (!gd) {
        list_del(&dev->list);
        goto error2;
    }

    gd->major = tr->major;
	gd->first_minor = 0;
	gd->fops = &nand_blktrans_ops;

    snprintf(gd->disk_name, sizeof(gd->disk_name),
         "%s%c", tr->name, (1?'a':'0') + dev->devnum);
    set_capacity(gd, 512);

    gd->private_data = dev;
    dev->disk = gd;
    gd->queue = dev->rq;
	
    dev->disable_access = 0;
    dev->readonly = 0;
    dev->writeonly = 0;

    mutex_init(&dev->lock);

    // Create the request queue
    spin_lock_init(&dev->queue_lock);
	dev->rq = blk_init_queue(null_for_dragonboard, &dev->queue_lock);
    if (!dev->rq){
       goto error3;
    }
    dev->rq->queuedata = dev;
    blk_queue_logical_block_size(dev->rq, tr->blksize);

    gd->queue = dev->rq;
	add_disk(gd);

    return 0;

error3:
    nand_dbg_err("\nerror3\n");
    put_disk(dev->disk);
error2:
    nand_dbg_err("\nerror2\n");
    list_del(&dev->list);
    return ret;
}


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int nand_blk_register(struct nand_blk_ops *tr)
{
    int ret;
    struct _nand_phy_partition* phy_partition;

    down(&nand_mutex);

    ret = register_blkdev(tr->major, tr->name);
    if(ret){
        nand_dbg_err("\nfaild to register blk device\n");
        up(&nand_mutex);
        return -1;
    }

    init_completion(&tr->thread_exit);
    init_waitqueue_head(&tr->thread_wq);
    sema_init(&tr->nand_ops_mutex, 1);

    //devfs_mk_dir(nandr->name);
    INIT_LIST_HEAD(&tr->devs);
    tr->nftl_blk_head.nftl_blk_next = NULL;
    tr->nand_dev_head.nand_dev_next = NULL;

    phy_partition = get_head_phy_partition_from_nand_info(p_nand_info);

    while(phy_partition != NULL)
    {
        tr->add_dev(tr, phy_partition);
        phy_partition = get_next_phy_partition(phy_partition);
    }

    up(&nand_mutex);

    return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void nand_blk_unregister(struct nand_blk_ops *tr)
{

    down(&nand_mutex);
    /* Clean up the kernel thread */
    tr->quit = 1;
    wake_up(&tr->thread_wq);
    wait_for_completion(&tr->thread_exit);

    /* Remove it from the list of active majors */
    tr->remove_dev(tr);

    unregister_blkdev(tr->major, tr->name);

    //devfs_remove(nandr->name);
    //blk_cleanup_queue(tr->rq);

    up(&nand_mutex);

    if (!list_empty(&tr->devs))
        BUG();
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
//int cal_partoff_within_disk(char *name,struct inode *i)
//{
//    struct gendisk *gd = i->i_bdev->bd_disk;
//    int current_minor = MINOR(i->i_bdev->bd_dev)  ;
//    int index = current_minor & ((1<<mytr.minorbits) - 1) ;
//    if(!index)
//        return 0;
//    return ( gd->part_tbl->part[ index - 1]->start_sect);
//}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int  init_blklayer(void)
{
#if 0
    script_item_u   good_block_ratio_flag;
    script_item_value_type_e  type;

    type = script_get_item("nand0_para", "good_block_ratio", &good_block_ratio_flag);

    if(SCIRPT_ITEM_VALUE_TYPE_INT != type)
        nand_dbg_inf("nand type err!\n");
    else
    {

    }
#endif
    return nand_blk_register(&mytr);
}

int init_blklayer_for_dragonboard(void)
{
    int ret;
	struct nand_blk_ops *tr;

	tr =  &mytr;

	dragonboard_test_flag = 1;

    down(&nand_mutex);

    ret = register_blkdev(tr->major, tr->name);
    if(ret){
        nand_dbg_err("\nfaild to register blk device\n");
        up(&nand_mutex);
        return -1;
    }

    init_completion(&tr->thread_exit);
    init_waitqueue_head(&tr->thread_wq);
    sema_init(&tr->nand_ops_mutex, 1);

    //devfs_mk_dir(nandr->name);
    INIT_LIST_HEAD(&tr->devs);
    tr->nftl_blk_head.nftl_blk_next = NULL;
    tr->nand_dev_head.nand_dev_next = NULL;

    tr->add_dev_test(tr);

    up(&nand_mutex);

    return 0;

}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void   exit_blklayer(void)
{
    nand_blk_unregister(&mytr);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int __init nand_drv_init(void)
{
    //printk("[NAND]2013-8-8 10:05\n");
    return init_blklayer();
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void __exit nand_drv_exit(void)
{
    exit_blklayer();

}

//module_init(nand_drv_init);
//module_exit(nand_drv_exit);
MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("nand flash groups");
MODULE_DESCRIPTION ("Generic NAND flash driver code");
