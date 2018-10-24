#define PROGRAM_NAME "nand_set_badblock_half_partiton"

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
#include <mtd/mtd-user.h>
#include "common.h"
#include <libmtd.h>
#define GOOD_BLOCK 1
#define BAD_BLOCK 0

struct mtd_dev_info mtd;
struct mtd_info_user meminfo;
struct mtd_ecc_stats oldstats, newstats;
struct mtd_oob_buf oob;
libmtd_t mtd_desc;
int fd;
int block_status=0;

static NORETURN void usage(int status)
{
	fprintf(status ? stderr : stdout,
		"Caution! This tool use to make half of the given Nandflash partiton as bad block, it will corrupt the partition's data! \n"
		"usage: %s [OPTIONS] <device> e.g. %s /dev/mtd5\n\n"
		"  -h, --help           Display this help output\n"
		"  -V, --version        Display version information and exit\n",
		PROGRAM_NAME,PROGRAM_NAME);
	exit(status);
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
Description:
    Here we manuly wirte the oob 
*/
static void mark_good_block(libmtd_t desc, const struct mtd_dev_info *mtd, int fd, uint32_t offs)
{
    /*1st and 2sec byte set to 0x00 consider a bad block*/
    const unsigned char oobbuf[2]={0xff,0xff};

    /* Write out oob data */
    mtd_write(mtd_desc, mtd, fd, offs / mtd->eb_size, offs % mtd->eb_size, NULL, 0, &oobbuf, sizeof(oobbuf), MTD_OPS_PLACE_OOB);

}


/*
 * Main program
 */
int main(int argc, char **argv)
{
	uint32_t offset = 0;
	int error = 0;
    loff_t address_ofs;



	for (;;) {
		static const char short_options[] = "h:s:V";
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
		}
    }
	if (argc - optind != 1)
		usage(EXIT_FAILURE);
	if (error)
		errmsg_die("Try --help for more information");

	fd = open(argv[optind], O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

    mtd_desc = libmtd_open();
    if (!mtd_desc)           
        errmsg_die("can't initialize libmtd");
                                              
    /* Fill in MTD device capability structure */
    if(mtd_get_dev_info(mtd_desc, argv[optind], &mtd) < 0)
        errmsg_die("mtd_get_dev_info failed");

	if (ioctl(fd, MEMGETINFO, &meminfo)) {
		perror("MEMGETINFO");
		close(fd);
		exit(1);
	}

	if (offset % meminfo.erasesize) {
		fprintf(stderr, "Offset %x not multiple of erase size %x\n",
			offset, meminfo.erasesize);
		exit(1);
	}


	if (ioctl(fd, ECCGETSTATS, &oldstats)) {
		perror("ECCGETSTATS");
		close(fd);
		exit(1);
	}

	printf("ECC corrections: %d\n", oldstats.corrected);
	printf("ECC failures   : %d\n", oldstats.failed);
	printf("Bad blocks     : %d\n", oldstats.badblocks);
	printf("BBT blocks     : %d\n", oldstats.bbtblocks);
    printf("DEBUG---------->block_status=%d\n",block_status);

    for(address_ofs=0; address_ofs < meminfo.size; address_ofs+=(meminfo.erasesize << 1)) //meminfo.erasesize *2
    {
        printf("Mark block bad at %08lx\n", (long)address_ofs);
        if(ioctl(fd, MEMSETBADBLOCK, &address_ofs) != 0 )
            printf("Mark block fail!\n");
        
    }
	/* Return happy */
	return 0;
}
