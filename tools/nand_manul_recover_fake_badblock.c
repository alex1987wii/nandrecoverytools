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

static NORETURN void usage(int status)
{
	fprintf(status ? stderr : stdout,
		"usage: %s [OPTIONS] <device>\n\n"
		"  -h, --help           Display this help output\n"
		"  -V, --version        Display version information and exit\n"
		"  -o, --offset         Start offset on flash\n",
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
struct mtd_ecc_stats oldstats, newstats;
unsigned char *buf;
struct INFTLMediaHeader *mh;
struct mtd_oob_buf oob;
int fd;
int markbad=0;
int seed;

static int read_and_compare(uint32_t ofs, unsigned char *data,
			    unsigned char *rbuf)
{
	ssize_t len;
	int i;

	//len = pread(fd, rbuf, meminfo.erasesize, ofs);
	len = pread(fd, rbuf, meminfo.erasesize,ofs);

	if (len < meminfo.erasesize) {
		printf("JK-------->Read error!\n");
		if (len)
			fprintf(stderr, "Short read (%zd bytes)\n", len);
		else
			perror("read");
		exit(1);
	}

	if (ioctl(fd, ECCGETSTATS, &newstats)) {
		printf("\n");
		perror("ECCGETSTATS");
		close(fd);
		exit(1);
	}

	if (newstats.corrected > oldstats.corrected) {
		printf("\n %d bit(s) ECC corrected at %08x\n",
				newstats.corrected - oldstats.corrected,
				(unsigned) ofs);
		oldstats.corrected = newstats.corrected;
	}
	if (newstats.failed > oldstats.failed) {
		printf("\nECC failed at %08x\n", (unsigned) ofs);
		oldstats.failed = newstats.failed;
	}

	printf("\r%08x: checking...", (unsigned)ofs);
	fflush(stdout);

	if (memcmp(data, rbuf, meminfo.erasesize)) {
		printf("\n");
		fprintf(stderr, "compare failed. seed %d\n", seed);
		//for (i=0; i<meminfo.erasesize; i++) {
		for (i=0; i<32; i++) {
			if (data[i] != rbuf[i])
				printf("Byte 0x%x is %02x should be %02x\n",
				       i, rbuf[i], data[i]);
		}
		return 1;
	}
	return 0;
}

static int erase_and_write(uint32_t ofs, unsigned char *data, unsigned char *rbuf,
			   int nr_reads)
{
	struct erase_info_user er;
	ssize_t len;
	int i, read_errs = 0;

	//printf("\r%08x: erasing... \n", (unsigned)ofs);
	//fflush(stdout);

	er.start = ofs;
	er.length = meminfo.erasesize;

	if (ioctl(fd, MEMERASEFORCE, &er)) {
		perror("MEMERASE");
		if (markbad) {
			printf("Mark block bad at %08lx\n", (long)ofs);
			ioctl(fd, MEMSETBADBLOCK, &ofs);
		}
		return 1;
	}


	//printf("\r%08x: writing...\n", (unsigned)ofs);
	//fflush(stdout);

	len = pwrite(fd, data, meminfo.erasesize, ofs);

	if (len < 0) {
		printf("JK-------->Write erro!\n");
		perror("write");
		if (markbad) {
			printf("Mark block bad at %08lx\n", (long)ofs);
			ioctl(fd, MEMSETBADBLOCK, &ofs);
		}
		return 1;
	}
	if (len < meminfo.erasesize) {
		printf("\n");
		fprintf(stderr, "Short write (%zd bytes)\n", len);
		exit(1);
	}

	for (i=1; i<=nr_reads; i++) {
		//printf("\r%08x: reading (%d of %d)...", (unsigned)ofs, i, nr_reads);
		//fflush(stdout);
		if (read_and_compare(ofs, data, rbuf))
			read_errs++;
	}
	if (read_errs) {
		fprintf(stderr, "read/check %d of %d failed. seed %d\n", read_errs, nr_reads, seed);
		return 1;
	}
	return 0;
}

static int block_forece_eraser(int fd, uint32_t offs)
{
	struct erase_info_user er;
	ssize_t len;

	er.start = offs;
	er.length = meminfo.erasesize;

	if (ioctl(fd, MEMERASEFORCE, &er)) {
		perror("MEMERASE");
		if (markbad) {
			printf("Mark block bad at %08lx\n", (long)offs);
			ioctl(fd, MEMSETBADBLOCK, &offs);
		}
		return -1;
	}

    return 0;
}


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
	struct mtd_dev_info mtd;
	unsigned char *wbuf, *rbuf, *kbuf;
	uint32_t offset = 0;
	uint32_t length = -1;
	int error = 0;


	seed = time(NULL);

	for (;;) {
		static const char short_options[] = "h:o:V";
		static const struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "version", no_argument, 0, 'V' },
			{ "offset", required_argument, 0, 'o' },
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

		case 'o':
			offset = simple_strtoul(optarg, &error);
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
    if (mtd_get_dev_info(mtd_desc, argv[optind], &mtd) < 0)
        errmsg_die("mtd_get_dev_info failed");

		printf("JK-------->mtd.name=%s\n",mtd.name);

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
