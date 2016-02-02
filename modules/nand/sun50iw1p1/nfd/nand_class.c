
#define _NAND_CLASS_C_

#include "nand_blk.h"
#include "nand_dev.h"


extern int debug_data;

extern uint32 gc_all(void* zone);
extern uint32 gc_one(void* zone);
extern uint32 prio_gc_one(void* zone,uint16 block,uint32 flag);
extern void print_nftl_zone(void * zone);
extern void print_free_list(void* zone);
extern void print_smart(void * zone);
extern void print_block_invalid_list(void* zone);
extern void print_logic_page_map(void * _zone,uint32 num);
extern uint32 nftl_set_zone_test(void * _zone,uint32 num);
extern int nand_dbg_phy_read(unsigned short nDieNum, unsigned short nBlkNum, unsigned short nPage);
extern int nand_dbg_zone_phy_read(void *zone,uint16 block,uint16 page);
extern int nand_dbg_zone_erase(void *zone,uint16 block,uint16 erase_num);
extern int nand_dbg_zone_phy_write(void *zone,uint16 block,uint16 page);
extern int nand_dbg_phy_write(unsigned short nDieNum, unsigned short nBlkNum, unsigned short nPage);
extern int nand_dbg_phy_erase(unsigned short nDieNum, unsigned short nBlkNum);
extern int _dev_nand_read2(char * name,__u32 start_sector,__u32 len,unsigned char *buf);

extern void nand_phy_test(void);
extern int nand_check_table(void *zone);

extern void udisk_test_start(struct _nftl_blk* nftl_blk);
extern void udisk_test_stop(void);


static ssize_t nand_test_store(struct kobject *kobject,struct attribute *attr, const char *buf, size_t count);
static ssize_t nand_test_show(struct kobject *kobject,struct attribute *attr, char *buf);
void obj_test_release(struct kobject *kobject);
void udisk_test_speed(struct _nftl_blk* nftl_blk);



int g_iShowVar = -1;

struct attribute prompt_attr = {
    .name = "nand_debug",
    .mode = S_IRUGO|S_IWUSR|S_IWGRP|S_IROTH
};

static struct attribute *def_attrs[] = {
    &prompt_attr,
    NULL
};


struct sysfs_ops obj_test_sysops =
{
    .show =  nand_test_show,
    .store = nand_test_store
};

struct kobj_type ktype =
{
    .release = obj_test_release,
    .sysfs_ops=&obj_test_sysops,
    .default_attrs=def_attrs
};

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void obj_test_release(struct kobject *kobject)
{
    nand_dbg_err("release");
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static ssize_t nand_test_show(struct kobject *kobject,struct attribute *attr, char *buf)
{
    ssize_t count = 0;
    struct nand_kobject* nand_kobj;

    nand_kobj = (struct nand_kobject*)kobject;

    print_nftl_zone(nand_kobj->nftl_blk->nftl_zone);

    count = sprintf(buf, "%i", g_iShowVar);

    return count;
}

/*****************************************************************************
*Name         :
*Description  :receive testcase num from echo command
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static ssize_t nand_test_store(struct kobject *kobject,struct attribute *attr, const char *buf, size_t count)
{
    int             ret,i;
    int             argnum = 0;
    char            cmd[32] = {0};
    unsigned int    param0 = 0;
    unsigned int    param1 = 0;
    unsigned int    param2 = 0;
    char            param3[16] = {0};
    char*           tempbuf;

    struct nand_kobject* nand_kobj;
    nand_kobj = (struct nand_kobject*)kobject;

    g_iShowVar = -1;

    argnum = sscanf(buf, "%s %u %u %u %s", cmd, &param0, &param1, &param2,param3);
    nand_dbg_err("argnum=%i, cmd=%s, param0=%u, param1=%u, param2=%u\n", argnum, cmd, param0, param1, param2);

    if (-1 == argnum)
    {
        nand_dbg_err("cmd format err!");
        g_iShowVar = -3;
        goto NAND_TEST_STORE_EXIT;
    }

    if(strcmp(cmd,"help") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  2013-10-24 19:48 \n");
    }
    else if(strcmp(cmd,"flush") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  flush \n");
        mutex_lock(nand_kobj->nftl_blk->blk_lock);
        ret = nand_kobj->nftl_blk->flush_write_cache(nand_kobj->nftl_blk,param0);
        mutex_unlock(nand_kobj->nftl_blk->blk_lock);
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"gcall") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  gcall \n");
        mutex_lock(nand_kobj->nftl_blk->blk_lock);
        ret = gc_all(nand_kobj->nftl_blk->nftl_zone);
        mutex_unlock(nand_kobj->nftl_blk->blk_lock);
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"gcone") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  gcone \n");
        mutex_lock(nand_kobj->nftl_blk->blk_lock);
        ret = gc_one(nand_kobj->nftl_blk->nftl_zone);
        mutex_unlock(nand_kobj->nftl_blk->blk_lock);
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"priogc") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  priogc \n");
        mutex_lock(nand_kobj->nftl_blk->blk_lock);
        ret = prio_gc_one(nand_kobj->nftl_blk->nftl_zone,param0,param1);
        mutex_unlock(nand_kobj->nftl_blk->blk_lock);
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"test") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  test \n");
        mutex_lock(nand_kobj->nftl_blk->blk_lock);
        ret = nftl_set_zone_test((void*)nand_kobj->nftl_blk->nftl_zone,param0);
        mutex_unlock(nand_kobj->nftl_blk->blk_lock);
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"showall") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  show all \n");
        print_free_list(nand_kobj->nftl_blk->nftl_zone);
        print_block_invalid_list(nand_kobj->nftl_blk->nftl_zone);
        //print_logic_page_map(nand_kobj->nftl_blk->nftl_zone,0);
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"showinfo") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  show info \n");
        print_nftl_zone(nand_kobj->nftl_blk->nftl_zone);
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"blkdebug") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("  blk debug \n");
        debug_data = param0;
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"smart") == 0)
    {
        nand_dbg_err("nand debug cmd:\n");
        nand_dbg_err("smart info\n");
        print_smart(nand_kobj->nftl_blk->nftl_zone);
        goto NAND_TEST_STORE_EXIT;
    }
    else if(strcmp(cmd,"read1") == 0)
    {
        nand_dbg_err("nand read1 cmd:\n");
        nand_dbg_phy_read(param0,param1,param2);
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"read2") == 0)
    {
        nand_dbg_err("nand read2 cmd:\n");
        nand_dbg_zone_phy_read(nand_kobj->nftl_blk->nftl_zone,param0,param1);
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"erase1") == 0)
    {
        nand_dbg_err("nand erase1 cmd:\n");
        nand_dbg_phy_erase(param0,param1);
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"erase2") == 0)
    {
        nand_dbg_err("nand erase2 cmd:\n");
        nand_dbg_zone_erase(nand_kobj->nftl_blk->nftl_zone,param0,param1);
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"write1") == 0)
    {
        nand_dbg_err("nand read1 cmd:\n");
        nand_dbg_phy_write(param0,param1,param2);
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"write2") == 0)
    {
        nand_dbg_err("nand read2 cmd:\n");
        nand_dbg_zone_phy_write(nand_kobj->nftl_blk->nftl_zone,param0,param1);
        goto NAND_TEST_STORE_EXIT;
    }

//    else if(strcmp(cmd,"phytest") == 0)
//    {
//        nand_dbg_err("nand read2 cmd:\n");
//        nand_phy_test();
//        goto NAND_TEST_STORE_EXIT;
//    }

    else if(strcmp(cmd,"checktable") == 0)
    {
        nand_dbg_err("nand read2 cmd:\n");

        mutex_lock(nand_kobj->nftl_blk->blk_lock);
        nand_check_table(nand_kobj->nftl_blk->nftl_zone);
        mutex_unlock(nand_kobj->nftl_blk->blk_lock);

        goto NAND_TEST_STORE_EXIT;
    }


    else if(strcmp(cmd,"testspeed") == 0)
    {
        nand_dbg_err("nand runtest cmd:\n");
        //kthread_run(nand_test_thread, nand_kobj->nftl_blk, "%sd", "nftl");
        udisk_test_speed(nand_kobj->nftl_blk);

        goto NAND_TEST_STORE_EXIT;
    }


    else if(strcmp(cmd,"teststart") == 0)
    {
        nand_dbg_err("nand runtest cmd:\n");
        udisk_test_start(nand_kobj->nftl_blk);
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"teststop") == 0)
    {
        nand_dbg_err("nand runtest cmd:\n");
        udisk_test_stop();
        goto NAND_TEST_STORE_EXIT;
    }

    else if(strcmp(cmd,"readdev") == 0)
    {
        nand_dbg_err("nand dev read\n");
        if(param1 > 16)
        {
            nand_dbg_err("max len is 16!\n");
            goto NAND_TEST_STORE_EXIT;
        }
        mutex_lock(nand_kobj->nftl_blk->blk_lock);

        tempbuf = kmalloc(8192,GFP_KERNEL);
        _dev_nand_read2(param3,param0,param1,tempbuf);
        for(i=0;i<(param1<<9);i+=4)
        {
            printk("%8x ", *((int*)&tempbuf[i]));
            if(((i+4)%64) == 0)
            {
                printk("\n");
            }
        }
        kfree(tempbuf);

        mutex_unlock(nand_kobj->nftl_blk->blk_lock);

        goto NAND_TEST_STORE_EXIT;
    }
    else
    {
        nand_dbg_err("err, nand debug undefined cmd: %s\n", cmd);
    }

NAND_TEST_STORE_EXIT:
    return count;
}


extern int _dev_nand_read2(char * name,unsigned int start_sector,unsigned int len,unsigned char *buf);
extern int _dev_nand_write2(char * name,unsigned int start_sector,unsigned int len,unsigned char *buf);

struct timeval tpstart2,tpend2;
long timeuse2;

void udisk_test_speed(struct _nftl_blk* nftl_blk)
{
    char* test_buf;

    unsigned int start_sector;
    unsigned int len;
    unsigned int i;
    unsigned int sec;
    int usec;

    test_buf = kmalloc(0x100000, GFP_KERNEL);  //1M

    do_gettimeofday(&tpstart2);

    start_sector = 0x32000;
    len = 0x800;
    for(i=0;i<500;i++)
    {
        mutex_lock(nftl_blk->blk_lock);
        _dev_nand_write2("UDISK",start_sector+i*len,len,test_buf);
        mutex_unlock(nftl_blk->blk_lock);
    }

    do_gettimeofday(&tpend2);

    sec = tpend2.tv_sec-tpstart2.tv_sec;
    usec = tpend2.tv_usec-tpstart2.tv_usec;

    printk("write sec:%d  usec:%d\n",sec,usec);

//======================================================================

    do_gettimeofday(&tpstart2);

    start_sector = 0x32000;
    len = 0x800;
    for(i=0;i<500;i++)
    {
        mutex_lock(nftl_blk->blk_lock);
        _dev_nand_read2("UDISK",start_sector+i*len,len,test_buf);
        mutex_unlock(nftl_blk->blk_lock);
    }

    do_gettimeofday(&tpend2);

    sec = tpend2.tv_sec-tpstart2.tv_sec;
    usec = tpend2.tv_usec-tpstart2.tv_usec;

    printk("read sec:%d  usec:%d\n",sec,usec);

    kfree(test_buf);
    return;
}
