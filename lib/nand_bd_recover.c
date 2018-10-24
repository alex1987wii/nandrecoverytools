#define PROGRAM_NAME "nandtest"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <getopt.h>

#include <asm/types.h>
#include "mtd/mtd-user.h"
#include <common.h>
#include <libmtd.h>
#include <network_library.h>
#include <nand_bd_recover.h>
#include <pthread.h>
#include <error_code.h>

#define NAND_BAD_BLOCK_MAXIMUM 1024
#define NAND_DEVICE_NAME_LENGTH 16
#define MTD_DEVICE_MAX 15


struct bad_block_unit{
    uint8_t device_name[NAND_DEVICE_NAME_LENGTH];
    uint32_t offset_address;
};

struct nand_bad_block_info{
    uint32_t index;
    uint32_t bad_block_num;
    struct bad_block_unit bad_block[NAND_BAD_BLOCK_MAXIMUM];
};


libmtd_t mtd_desc;
struct mtd_info_user meminfo;
struct mtd_dev_info mtd;
struct mtd_ecc_stats oldstats, newstats;
unsigned char *buf;
struct INFTLMediaHeader *mh;
struct mtd_oob_buf oob;
struct nand_bad_block_info *bb_info;
int fd;
uint8_t device_path[16];
pthread_mutex_t mutex_bb_recover_num = PTHREAD_MUTEX_INITIALIZER;
uint32_t g_bb_recover_num=0;

/*
return 1 indicates found target partition
*/
static int is_target_partition(char *device_path, char *partition_name)
{
    
	NAND_DEBUG("NAND_DEBUG-------->device_path=%s\n",device_path);

	fd = open(device_path, O_RDWR);
	if (fd < 0) {
		perror("open");
        return ERROR_CODE_OPEN_FILE_ERROR;
	}

    /* Fill in MTD device capability structure */
    if(mtd_get_dev_info(mtd_desc, device_path, &mtd) < 0)
        return ERROR_CODE_OPEN_MTD_ERROR;


	NAND_DEBUG("NAND_DEBUG-------->mtd.name=%s\n",mtd.name);
    if( memcmp(mtd.name, partition_name, strlen(partition_name)) ==0 )
    {
	    NAND_DEBUG("NAND_DEBUG-------->Found target partition!\n");
        close(fd);
        return 1;
    }
    else 
        close(fd);
    return -1;
}

static int block_forece_eraser(int fd, uint32_t offs)
{
    struct mtd_info_user mtd_meminfo;
	struct erase_info_user er;

	if (ioctl(fd, MEMGETINFO, &mtd_meminfo)) {
		perror("MEMGETINFO");
		close(fd);
		exit(1);
	}

	er.start = offs;
	er.length = mtd_meminfo.erasesize;

	if (ioctl(fd, MEMERASEFORCE, &er))
		return -1;

    return 0;
}

/*
Description:
    Here we manuly wirte the oob 
*/
static void mark_bad_block(libmtd_t desc, const struct mtd_dev_info *mtd, int fd, uint32_t offs)
{
    /*1st and 2sec byte set to none 0xFF consider a bad block*/
    const unsigned char oobbuf[2]={0x00,0x00};

    /* Write out oob data */
    mtd_write(mtd_desc, mtd, fd, offs / mtd->eb_size, offs % mtd->eb_size, NULL, 0, &oobbuf, sizeof(oobbuf), MTD_OPS_PLACE_OOB);

}


/*
 * get_bd_number
 * Get the number of badblock in a specify partition.
 * @partiton_name: name by given partiton, such as "rootfs" "user data"
 * use cammand "cat /proc/mtd" to get avaliable partition name 
 */

int get_bd_number(unsigned char *partition_name)
{
    unsigned int bd_number=0;
	int i, ret, badblock;
	uint32_t offset = 0;
    bool found_target_partition=false;

    mtd_desc = libmtd_open(); 
    if (!mtd_desc)
    {
        NAND_DEBUG("NAND_DEBUG--------->Fail to init libmtd! error code:%d", ERROR_CODE_INIT_LIBMTD_ERROR);
        return ERROR_CODE_INIT_LIBMTD_ERROR; 
    }

    NAND_DEBUG("partiton_name:%s\n", partition_name);
    /*Find out "@partiton_name" partition, if so, find out how many badblock it has*/
    for(i = 0; i < MTD_DEVICE_MAX;  i++)
    {
        sprintf(device_path,"/dev/mtd%d",i);
        if (0 == access(device_path,0))
        {
            ret=is_target_partition(device_path, partition_name);
            if(ret==1)
            {
	            NAND_DEBUG("NAND_DEBUG--------> Start scanning badblock\n");
                fd = open(device_path, O_RDWR);
                if (fd < 0) {
                    perror("open");
                    return ERROR_CODE_OPEN_FILE_ERROR;
                }

                /* Fill in MTD device capability structure */
                if(mtd_get_dev_info(mtd_desc, device_path, &mtd) < 0)
                    return ERROR_CODE_OPEN_MTD_ERROR;

                found_target_partition=true;
                /*find out all blocks that marks bad*/         
                for(offset=0; offset < mtd.size; offset +=mtd.eb_size)
                {
                    badblock = mtd_is_bad(&mtd, fd, offset / mtd.eb_size);

                    /*do recover*/
                    if(badblock)
                        bd_number++;
                }
                /*Alreday found the target partition and finished job, break loop*/
	            NAND_DEBUG("NAND_DEBUG--------> Finished seraching\n");
                break;
            }

        }
    }

    libmtd_close(mtd_desc); 

    if(found_target_partition)
    {
        /*close the file discriptor that opend*/
        close(fd);

	    /* Return happy */
        return bd_number;
    }
    else
        printf("There's no partition name as:%s !!\n",partition_name); 
    return -1;
}


/*
 * nand_bad_recover
 * Find out all badblock in a given specify partition, try to recover it.
 * @partiton_name: name by given partiton, such as "rootfs" "user data"
 * use cammand "cat /proc/mtd" to get avaliable partition name 
 */
int nand_bd_recover(unsigned char *partition_name)
{
	int i, ret, badblock;
	uint32_t offset = 0;
    bool found_target_partition=false;

    NAND_DEBUG("partiton_name:%s\n",partition_name);


    mtd_desc = libmtd_open(); 
    if (!mtd_desc)
        errmsg_die("can't initialize libmtd"); 

    /*Find out "@partiton_name" partition, if so, do reconver precedure*/
    for(i = 0; i < MTD_DEVICE_MAX;  i++)
    {
        sprintf(device_path,"/dev/mtd%d",i);
        if (0 == access(device_path,0))
        {
            ret=is_target_partition(device_path, partition_name);
            if(ret==1)
            {
                fd = open(device_path, O_RDWR);
                if (fd < 0) {
                    perror("open");
                    return ERROR_CODE_OPEN_FILE_ERROR;
                }
                /* Fill in MTD device capability structure */
                if(mtd_get_dev_info(mtd_desc, device_path, &mtd) < 0)
                    errmsg_die("mtd_get_dev_info failed");

                found_target_partition=true;

                /*find out all blocks that marks bad*/         
                for(offset=0; offset < mtd.size; offset +=mtd.eb_size)
                {

                    badblock = mtd_is_bad(&mtd, fd, offset / mtd.eb_size);

                    /*do recover*/
                    if(badblock)
                    {
                        NAND_DEBUG("\nFound bad block in \"%s\" partiton, offset:0x%0x. ",mtd.name,offset);
                        
                        /*block_forece_eraser, then do write/read test*/
                        if(mtd_torture(mtd_desc, &mtd, fd, offset/mtd.eb_size) != 0 )
                        {
                            /*Either eraser or write/read test failed, consider it's true bad block so mark it bad*/
                            mark_bad_block(mtd_desc, &mtd, fd, offset);
                            printf("This is a TURE BAD BLOCK, remain it as bad!\n");
                        }
                        else
                        {
                            /*Pass the test, it's a good block*/
                            block_forece_eraser(fd,offset);
                            pthread_mutex_lock( &mutex_bb_recover_num );
                            g_bb_recover_num++;
                            pthread_mutex_unlock( &mutex_bb_recover_num);
                            NAND_DEBUG("Block in \"%s\" partiton, offset:0x%0x recover succeed!\n", mtd.name, offset);
                            ;

                        }
                    }
        
                }
            }
        }
    
    } 

    libmtd_close(mtd_desc); 

    if(found_target_partition)
    {
        /*close the file discriptor that opend*/
        close(fd);
	    /* Return happy */
        return 0;
    }
    else
        printf("DBG----------> There's no partition name as:%s !!\n",partition_name); 
    return ERROR_CODE_NO_SUCH_PARTITION;


}
