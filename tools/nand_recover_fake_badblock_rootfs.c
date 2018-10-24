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
#include "common.h"
#include <libmtd.h>

#define NAND_BAD_BLOCK_MAXIMUM 1024
#define NAND_DEVICE_NAME_LENGTH 16
#define MTD_DEVICE_MAX 15

static NORETURN void usage(int status)
{
	fprintf(status ? stderr : stdout,
		"This utility is used to recover fake bad block(s) in 'Root file system' partition\n\n"
		"  -h, --help           Display this help output\n"
		PROGRAM_NAME);
	exit(status);
}

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
int markbad=0;

/*
return 0 indicates found target partition
*/
static int is_target_partition(char *device_path, char *partition_name)
{
    
	printf("JK-------->device_path=%s\n",device_path);

	fd = open(device_path, O_RDWR);
	if (fd < 0) {
		perror("open");
        return -1;
	}

    /* Fill in MTD device capability structure */
    if(mtd_get_dev_info(mtd_desc, device_path, &mtd) < 0)
        errmsg_die("mtd_get_dev_info failed");

	printf("JK-------->mtd.name=%s\n",mtd.name);
    if( memcmp(mtd.name,"Root file system",sizeof("Root file system")) ==0 )
    {
	    printf("JK-------->Found target partition!\n",mtd.name);
        return 0;
    }
    else 
        close(fd);
    return -1;
}

static int block_forece_eraser(int fd, uint32_t offs)
{
    struct mtd_info_user mtd_meminfo;
	struct erase_info_user er;
	ssize_t len;

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
    /*1st and 2sec byte set to 0x00 consider a bad block*/
    const unsigned char oobbuf[2]={0x00,0x00};

    /* Write out oob data */
    mtd_write(mtd_desc, mtd, fd, offs / mtd->eb_size, offs % mtd->eb_size, NULL, 0, &oobbuf, sizeof(oobbuf), MTD_OPS_PLACE_OOB);

}


/*
 * Main program
 */
int main(int argc, char **argv)
{
	int i, ret, badblock;
	unsigned char *wbuf, *rbuf, *kbuf;
	uint32_t offset = 0;
	uint32_t length = -1;
	int error = 0;


	for (;;) {
		static const char short_options[] = "h:V";
		static const struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "version", no_argument, 0, 'V' },
			{0, 0, 0, 0},
		};
		int option_index = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == EOF)
			break;

		switch (c) {
		case 'h':
			usage(EXIT_SUCCESS);
		case 'V':
			common_print_version();
			exit(EXIT_SUCCESS);
			break;
		case '?':
			usage(EXIT_FAILURE);


		}
	}
    /*
	if (argc - optind != 1)
		usage(EXIT_FAILURE);
	if (error)
		errmsg_die("Try --help for more information");
    */

    bb_info=malloc(sizeof(struct nand_bad_block_info));
    if(bb_info==NULL)
    {
       fprintf(stderr,"Can't allocate enough memory\n");
        return -1;
    }

    mtd_desc = libmtd_open(); 
    if (!mtd_desc)
        errmsg_die("can't initialize libmtd"); 

    /*Find out "user data" partition, if so, do reconver precedure*/
    for(i = 0; i < MTD_DEVICE_MAX;  i++)
    {
        sprintf(device_path,"/dev/mtd%d",i);
        if (0 == access(device_path,0))
        {
            ret=is_target_partition(device_path, "user data");
            if(!ret)
            {
                /*find out all blocks that marks bad*/         
                for(offset=0; offset < mtd.size; offset +=mtd.eb_size)
                {
                    badblock = mtd_is_bad(&mtd, fd, offset / mtd.eb_size);

                    /*do recover*/
                    if(badblock)
                    {
                        printf("\nFound bad block in \"%s\" partiton, offset:0x%0x. ",mtd.name,offset);
                        /*Erase the bad block and test it */
	                    ret = ioctl(fd, MEMERASEFORCE, offset);
                        if (ret < 0)
                        {
                            /*Force Erase failed, it's a ture bad block so we move on to next */
                            printf("\nBlock in \"%s\" partiton, offset:0x%0x force erase failed!\n",mtd.name,offset);
                            continue;
                        }

                        if(mtd_torture(mtd_desc, &mtd, fd, offset/mtd.eb_size) != 0 )
                        {
                            /*True bad block, mark as bad*/
                            mark_bad_block(mtd_desc, &mtd, fd, offset);
                            printf("This is a TURE BAD BLOCK, remain it as bad!\n", mtd.name, offset);
                        }
                        else
                        {
                            /*Pass the test, it's a good block*/
                            block_forece_eraser(fd,offset);
                            printf("Block in \"%s\" partiton, offset:0x%0x recover succeed!\n", mtd.name, offset);

                        }
                    }
        
                }
            }
        }
    
    } 

    
    return 0;


	if (ioctl(fd, MEMGETINFO, &meminfo)) {
		perror("MEMGETINFO");
		close(fd);
		exit(1);
	}

	length = meminfo.erasesize;

	if (offset % meminfo.erasesize) {
		fprintf(stderr, "Offset %x not multiple of erase size %x\n",
			offset, meminfo.erasesize);
		exit(1);
	}

	if (length + offset > meminfo.size) {
		fprintf(stderr, "Length %x + offset %x exceeds device size %x\n",
			length, offset, meminfo.size);
		exit(1);
	}

	wbuf = malloc(meminfo.erasesize * 2);
	if (!wbuf) {
		fprintf(stderr, "Could not allocate %d bytes for data buffer\n",
			meminfo.erasesize * 2);
		exit(1);
	}

	if (ioctl(fd, ECCGETSTATS, &oldstats)) {
		perror("ECCGETSTATS");
		close(fd);
		exit(1);
	}

    fprintf(stderr, "ECC failed: %d\n", oldstats.failed);
    fprintf(stderr, "ECC corrected: %d\n", oldstats.corrected);
    fprintf(stderr, "Number of bad blocks: %d\n", oldstats.badblocks);
    fprintf(stderr, "Number of bbt blocks: %d\n", oldstats.bbtblocks);

    
    block_forece_eraser(fd, offset);

    /*Fake bad block*/
    if(mtd_torture(mtd_desc, &mtd, fd, offset/mtd.eb_size) != 0 )
    {
        mark_bad_block(mtd_desc, &mtd, fd, offset);
    }

	if (ioctl(fd, ECCGETSTATS, &newstats)) {
		printf("\n");
		perror("ECCGETSTATS");
		close(fd);
		exit(1);
	}

	if (newstats.corrected > oldstats.corrected) {
		printf("\n %d bit(s) ECC corrected at %08x\n",
				newstats.corrected - oldstats.corrected,offset);
		oldstats.corrected = newstats.corrected;
	}
	if (newstats.failed > oldstats.failed) {
		printf("\nECC failed at %08x\n", offset);
		oldstats.failed = newstats.failed;
	}

	/* Return happy */
	return 0;

    //mark_bad_block(mtd_desc, &mtd, fd, offset);
    //return 0;

}
