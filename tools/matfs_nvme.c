#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/module.h>

// /dev/nvme0n1
#define DISK_NAME "/dev/nvme0n1"

static void
matfs_nvme_write(struct block_device* bdev, char* buf, int length)
{
    // printk(KERN_INFO "matfs_nvme_write in");
    struct page *page;

    struct bio* bio = bio_alloc(bdev, BIO_MAX_VECS, REQ_OP_WRITE, GFP_NOIO);
    bio->bi_iter.bi_sector = 1;
    page = alloc_page(GFP_KERNEL);

    if (!page) {
        printk(KERN_ERR "alloc_page failed\n");
    }

    memcpy(page_address(page), buf, length);
    printk(KERN_INFO "matfs_nvme_write page:%s", (char*)page_address(page));

    bio_add_page(bio, page, 512, 0);
    submit_bio_wait(bio);

    __free_page(page);
    bio_put(bio);
    // printk(KERN_INFO "matfs_nvme_write out");
}

static void
matfs_nvme_read(struct block_device* bdev, char* buf, int length)
{
    // printk(KERN_INFO "matfs_nvme_read in");
    struct page *page = NULL;

    struct bio* bio = bio_alloc(bdev, BIO_MAX_VECS, REQ_OP_READ, GFP_NOIO);
    bio->bi_iter.bi_sector = 1;
    page = alloc_page(GFP_KERNEL);

    if (!page) {
        printk(KERN_ERR "alloc_page failed\n");
    }
    // bio->bi_private = page;
    bio_add_page(bio, page, 512, 0);
    submit_bio_wait(bio);

    memcpy(buf, page_address(page), length);

    printk(KERN_INFO "matfs_nvme_read page:%s", (char*)page_address(page));
    __free_page(page);
    bio_put(bio);
    // printk(KERN_INFO "matfs_nvme_read out");
}

// insmod matfs_nvme.ko
int
matfs_nvme_init(void)
{
    char *str = "1ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buffer[512] = {0};
    struct block_device* bdev;
    printk(KERN_INFO "matfs_nvme_init\n");

    bdev = blkdev_get_by_path(DISK_NAME, FMODE_READ | FMODE_WRITE, THIS_MODULE);

    if (!bdev) {
        printk(KERN_ERR "blkdev_get_by_path failed\n");
        return -EINVAL;
    }

    matfs_nvme_write(bdev, str, strlen(str));
    matfs_nvme_read(bdev, buffer, 512);

    printk(KERN_INFO "buffer: %s\n", buffer);
    return 0;
}

// rmmod matfs_nvme.ko
void
matfs_nvme_exit(void)
{
    
    printk(KERN_INFO "matfs_nvme_exit\n");
}

module_init(matfs_nvme_init);
module_exit(matfs_nvme_exit);
MODULE_LICENSE("GPL");