/*
 *  nanddump.c
 *
 *  Copyright (C) 2000 David Woodhouse (dwmw2@infradead.org)
 *                     Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This utility dumps the contents of raw NAND chips or NAND
 *   chips contained in DoC devices.
 */

#define PROGRAM_NAME "nanddump"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <asm/types.h>
#include <mtd/mtd-user.h>
#include "common.h"
#include "libmtd.h"
#define NAND_BAD_BLOCK_MAXIMUM 1024
#define NAND_DEVICE_NAME_LENGTH 16

struct nand_bad_block_info{
    uint8_t device_name[NAND_DEVICE_NAME_LENGTH];
    uint32_t offset_address;
};

struct nand_info{
    uint32_t index;
    uint32_t bad_block_num;
    struct nand_bad_block_info bad_block_info[NAND_BAD_BLOCK_MAXIMUM];
};

static void display_help(int status)
{
	fprintf(status == EXIT_SUCCESS ? stdout : stderr,
"Usage: %s [OPTIONS] MTD-device\n"
"Dumps the contents of a nand mtd partition.\n"
"\n"
"-h         --help               Display this help and exit\n"
"           --version            Output version information and exit\n"
"           --bb=METHOD          Choose bad block handling method (see below).\n"
"-a         --forcebinary        Force printing of binary data to tty\n"
"-c         --canonicalprint     Print canonical Hex+ASCII dump\n"
"-f file    --file=file          Dump to file\n"
"-l length  --length=length      Length\n"
"-n         --noecc              Read without error correction\n"
"           --omitoob            Omit OOB data (default)\n"
"-o         --oob                Dump OOB data\n"
"-p         --prettyprint        Print nice (hexdump)\n"
"-q         --quiet              Don't display progress and status messages\n"
"-s addr    --startaddress=addr  Start address\n"
"           --skip-bad-blocks-to-start\n"
"                                Skip bad blocks when seeking to the start address\n"
"\n"
"--bb=METHOD, where METHOD can be `padbad', `dumpbad', or `skipbad':\n"
"    padbad:  dump flash data, substituting 0xFF for any bad blocks\n"
"    dumpbad: dump flash data, including any bad blocks\n"
"    skipbad: dump good data, completely skipping any bad blocks (default)\n",
	PROGRAM_NAME);
	exit(status);
}

static void display_version(void)
{
	common_print_version();
	printf("%1$s comes with NO WARRANTY\n"
			"to the extent permitted by law.\n"
			"\n"
			"You may redistribute copies of %1$s\n"
			"under the terms of the GNU General Public Licence.\n"
			"See the file `COPYING' for more information.\n",
			PROGRAM_NAME);
	exit(EXIT_SUCCESS);
}

// Option variables

static bool			pretty_print = false;	// print nice
static bool			noecc = false;		// don't error correct
static bool			omitoob = true;		// omit oob data
static long long		start_addr;		// start address
static long long		length;			// dump length
static unsigned char		*mtddev;		// mtd device name
static const char		*dumpfile;		// dump file name
static bool			quiet = false;		// suppress diagnostic output
static bool			canonical = false;	// print nice + ascii
static bool			forcebinary = false;	// force printing binary to tty
static bool			skip_bad_blocks_to_start = false;

static enum {
	padbad,   // dump flash data, substituting 0xFF for any bad blocks
	dumpbad,  // dump flash data, including any bad blocks
	skipbad,  // dump good data, completely skipping any bad blocks
} bb_method = skipbad;

static void process_options(int argc, char * const argv[])
{
	int error = 0;
	bool oob_default = true;

	for (;;) {
		int option_index = 0;
		static const char short_options[] = "hs:f:l:opqncaV";
		static const struct option long_options[] = {
			{"version", no_argument, 0, 'V'},
			{"bb", required_argument, 0, 0},
			{"omitoob", no_argument, 0, 0},
			{"skip-bad-blocks-to-start", no_argument, 0, 0 },
			{"help", no_argument, 0, 'h'},
			{"forcebinary", no_argument, 0, 'a'},
			{"canonicalprint", no_argument, 0, 'c'},
			{"file", required_argument, 0, 'f'},
			{"oob", no_argument, 0, 'o'},
			{"prettyprint", no_argument, 0, 'p'},
			{"startaddress", required_argument, 0, 's'},
			{"length", required_argument, 0, 'l'},
			{"noecc", no_argument, 0, 'n'},
			{"quiet", no_argument, 0, 'q'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
			case 0:
				switch (option_index) {
					case 1:
						/* Handle --bb=METHOD */
						if (!strcmp(optarg, "padbad"))
							bb_method = padbad;
						else if (!strcmp(optarg, "dumpbad"))
							bb_method = dumpbad;
						else if (!strcmp(optarg, "skipbad"))
							bb_method = skipbad;
						else
							error++;
						break;
					case 2: /* --omitoob */
						if (oob_default) {
							oob_default = false;
							omitoob = true;
						} else {
							errmsg_die("--oob and --oomitoob are mutually exclusive");
						}
						break;
					case 3: /* --skip-bad-blocks-to-start */
						skip_bad_blocks_to_start = true;
						break;
				}
				break;
			case 'V':
				display_version();
				break;
			case 's':
				start_addr = simple_strtoll(optarg, &error);
				break;
			case 'f':
				dumpfile = xstrdup(optarg);
				break;
			case 'l':
				length = simple_strtoll(optarg, &error);
				break;
			case 'o':
				if (oob_default) {
					oob_default = false;
					omitoob = false;
				} else {
					errmsg_die("--oob and --oomitoob are mutually exclusive");
				}
				break;
			case 'a':
				forcebinary = true;
				break;
			case 'c':
				canonical = true;
				/* fall-through */
			case 'p':
				pretty_print = true;
				break;
			case 'q':
				quiet = true;
				break;
			case 'n':
				noecc = true;
				break;
			case 'h':
				display_help(EXIT_SUCCESS);
				break;
			case '?':
				error++;
				break;
		}
	}

	if (start_addr < 0)
		errmsg_die("Can't specify negative offset with option -s: %lld",
				start_addr);

	if (length < 0)
		errmsg_die("Can't specify negative length with option -l: %lld", length);

	if (quiet && pretty_print) {
		fprintf(stderr, "The quiet and pretty print options are mutually-\n"
				"exclusive. Choose one or the other.\n");
		exit(EXIT_FAILURE);
	}

	if (forcebinary && pretty_print) {
		fprintf(stderr, "The forcebinary and pretty print options are\n"
				"mutually-exclusive. Choose one or the "
				"other.\n");
		exit(EXIT_FAILURE);
	}

	if ((argc - optind) != 1 || error)
		display_help(EXIT_FAILURE);

	mtddev = argv[optind];
}

#define PRETTY_ROW_SIZE 16
#define PRETTY_BUF_LEN 80

/**
 * pretty_dump_to_buffer - formats a blob of data to "hex ASCII" in memory
 * @buf: data blob to dump
 * @len: number of bytes in the @buf
 * @linebuf: where to put the converted data
 * @linebuflen: total size of @linebuf, including space for terminating NULL
 * @pagedump: true - dumping as page format; false - dumping as OOB format
 * @ascii: dump ascii formatted data next to hexdump
 * @prefix: address to print before line in a page dump, ignored if !pagedump
 *
 * pretty_dump_to_buffer() works on one "line" of output at a time, i.e.,
 * PRETTY_ROW_SIZE bytes of input data converted to hex + ASCII output.
 *
 * Given a buffer of unsigned char data, pretty_dump_to_buffer() converts the
 * input data to a hex/ASCII dump at the supplied memory location. A prefix
 * is included based on whether we are dumping page or OOB data. The converted
 * output is always NULL-terminated.
 *
 * e.g.
 *   pretty_dump_to_buffer(data, data_len, prettybuf, linelen, true,
 *                         false, 256);
 * produces:
 *   0x00000100: 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f
 * NOTE: This function was adapted from linux kernel, "lib/hexdump.c"
 */
static void pretty_dump_to_buffer(const unsigned char *buf, size_t len,
		char *linebuf, size_t linebuflen, bool pagedump, bool ascii,
		unsigned long long prefix)
{
	static const char hex_asc[] = "0123456789abcdef";
	unsigned char ch;
	unsigned int j, lx = 0, ascii_column;

	if (pagedump)
		lx += sprintf(linebuf, "0x%.8llx: ", prefix);
	else
		lx += sprintf(linebuf, "  OOB Data: ");

	if (!len)
		goto nil;
	if (len > PRETTY_ROW_SIZE)	/* limit to one line at a time */
		len = PRETTY_ROW_SIZE;

	for (j = 0; (j < len) && (lx + 3) <= linebuflen; j++) {
		ch = buf[j];
		linebuf[lx++] = hex_asc[(ch & 0xf0) >> 4];
		linebuf[lx++] = hex_asc[ch & 0x0f];
		linebuf[lx++] = ' ';
	}
	if (j)
		lx--;

	ascii_column = 3 * PRETTY_ROW_SIZE + 14;

	if (!ascii)
		goto nil;

	/* Spacing between hex and ASCII - ensure at least one space */
	lx += sprintf(linebuf + lx, "%*s",
			MAX((int)MIN(linebuflen, ascii_column) - 1 - lx, 1),
			" ");

	linebuf[lx++] = '|';
	for (j = 0; (j < len) && (lx + 2) < linebuflen; j++)
		linebuf[lx++] = (isascii(buf[j]) && isprint(buf[j])) ? buf[j]
			: '.';
	linebuf[lx++] = '|';
nil:
	linebuf[lx++] = '\n';
	linebuf[lx++] = '\0';
}

/**
 * ofd_write - writes whole buffer to the file associated with a descriptor
 *
 * On failure an error (negative number) is returned. Otherwise 0 is returned.
 */
static int ofd_write(int ofd, const void *buf, size_t nbyte)
{
	const unsigned char *data = buf;
	ssize_t bytes;

	while (nbyte) {
		bytes = write(ofd, data, nbyte);
		if (bytes < 0) {
			int err = -errno;

			sys_errmsg("Unable to write to output");

			return err;
		}
		data += bytes;
		nbyte -= bytes;
	}

	return 0;
}

/*
 * Main program
 */
int nand_do_bad_block_scann(unsigned char *dev, struct nand_info *nand_info)
{
	long long ofs, end_addr = 0;
	long long blockstart = 1;
	int i, fd, ofd = 0, bs, badblock = 0;
	struct mtd_dev_info mtd;
	char pretty_buf[PRETTY_BUF_LEN];
	int firstblock = 1;
	struct mtd_ecc_stats stat1, stat2;
	bool eccstats = false;
	unsigned char *readbuf = NULL, *oobbuf = NULL;
	libmtd_t mtd_desc;
	int err;
    quiet=true;

	//process_options(argc, argv);
	mtddev = dev;

	/* Initialize libmtd */
	mtd_desc = libmtd_open();
	if (!mtd_desc)
		return -1;
		//return errmsg("can't initialize libmtd");

	/* Open MTD device */
	if ((fd = open(mtddev, O_RDONLY)) == -1) {
		perror(mtddev);
		return -1;
	}

	/* Fill in MTD device capability structure */
	if (mtd_get_dev_info(mtd_desc, mtddev, &mtd) < 0)
		//return errmsg("mtd_get_dev_info failed");
		return -1;

	/* Allocate buffers */
	oobbuf = xmalloc(sizeof(oobbuf) * mtd.oob_size);
	readbuf = xmalloc(sizeof(readbuf) * mtd.min_io_size);

	/* check if we can read ecc stats */
	if (!ioctl(fd, ECCGETSTATS, &stat1)) {
		eccstats = true;
	} else
		perror("No ECC status information available");

	/* Initialize start/end addresses and block size */
	if (start_addr & (mtd.min_io_size - 1)) {
		fprintf(stderr, "the start address (-s parameter) is not page-aligned!\n"
				"The pagesize of this NAND Flash is 0x%x.\n",
				mtd.min_io_size);
		goto closeall;
	}


	if (length)
		end_addr = start_addr + length;
	if (!length || end_addr > mtd.size)
		end_addr = mtd.size;

	bs = mtd.min_io_size;

	/* Print informative message */
	if (!quiet) {
		fprintf(stderr, "Block size %d, page size %d, OOB size %d\n",
				mtd.eb_size, mtd.min_io_size, mtd.oob_size);
		fprintf(stderr,
				"Dumping data starting at 0x%08llx and ending at 0x%08llx...\n",
				start_addr, end_addr);
	}

	/* Dump the flash contents */
	//fprintf(stderr, "JK----------> start_addr=0x%08llx, ofs=0x%08llx, end_addr=0x%08llx, bs=%d\n",start_addr,ofs,end_addr,bs);
	for (ofs = start_addr; ofs < end_addr; ofs += bs) {
		/* Check for bad block */

		//fprintf(stderr, "JK----------> ofs=0x%08llx\n",ofs);

		badblock = mtd_is_bad(&mtd, fd, ofs / mtd.eb_size);

		if (badblock) {
			//fprintf(stderr,"Found bad block at %s, offset:0x%0llx\n",mtddev,ofs);
			// fill out nand_info
			memcpy(nand_info->bad_block_info[nand_info->bad_block_num].device_name, mtddev,NAND_DEVICE_NAME_LENGTH);
			//fprintf(stderr,"mtddev=%s,nand_info->bad_block_info[%d]=%s.\n",mtddev, nand_info->bad_block_num, nand_info->bad_block_info[nand_info->bad_block_num].device_name);
			nand_info->bad_block_info[nand_info->bad_block_num].offset_address=ofs;
			nand_info->bad_block_num++;
			/* skip bad block, increase end_addr */
			end_addr += mtd.eb_size;
			ofs += mtd.eb_size - bs;
			if (end_addr > mtd.size)
				end_addr = mtd.size;
			continue;
		}
		else if(badblock < 0) //Occure failure
		{
			errmsg("libmtd: mtd_is_bad");

		}

		/* ECC stats available ? */
		if (eccstats) {
			if (ioctl(fd, ECCGETSTATS, &stat2)) {
				perror("ioctl(ECCGETSTATS)");
				goto closeall;
			}
			if (stat1.failed != stat2.failed)
				fprintf(stderr, "ECC: %d uncorrectable bitflip(s)"
						" at offset 0x%08llx\n",
						stat2.failed - stat1.failed, ofs);
			if (stat1.corrected != stat2.corrected)
				fprintf(stderr, "ECC: %d corrected bitflip(s) at"
						" offset 0x%08llx\n",
						stat2.corrected - stat1.corrected, ofs);
			stat1 = stat2;
		}
	}

	/* Close the output file and MTD device, free memory */
	close(fd);
	close(ofd);
	free(oobbuf);
	free(readbuf);

	/* Exit happy */
	return EXIT_SUCCESS;

closeall:
	close(fd);
	close(ofd);
	free(oobbuf);
	free(readbuf);
	//exit(EXIT_FAILURE);
	return -1;
}

int main(int argc, unsigned char* argv[])
{
    
    uint32_t ret,refresh_time,test_time=0,tmp=0;
    struct nand_info *nand_info;
    uint8_t device_path[16];

    if(argc!= 1)
    {
        fprintf(stderr,"Usage: %s -t [SECONDS]\n"
        "-t         --time  refresh time in seconds(0~300)\n"
        ,argv[0]);
        exit(EXIT_SUCCESS);
    }

    refresh_time=3;
    //fprintf(stderr, "refresh_time=%d\n",refresh_time);
    if((refresh_time<1) || (refresh_time>300))
    {
        fprintf(stderr, "Input time not valid! range shoule be 1~300 in seconds.\n");
        exit(EXIT_SUCCESS);
    }


    nand_info=malloc(sizeof(struct nand_info));
    if(nand_info==NULL)
    {
       fprintf(stderr,"Can't allocate enough memory\n");
        return -1;
    }
    while(1)
    {
        nand_info->bad_block_num=0;

        for(tmp=0; tmp<10; tmp++)
        {
            sprintf(device_path,"/dev/mtd%d",tmp);
        if (0 == access(device_path,0))
            ret=nand_do_bad_block_scann(device_path,nand_info);
        }

        printf("Bad block(s)= %d, test_time=%d, refresh_time=%d seconds.\n"
                    ,nand_info->bad_block_num,++test_time,refresh_time);

        /*Print out bad block's details if bad block(s) has been found.*/
        for(tmp=0; tmp < nand_info->bad_block_num; tmp++)
        {

            printf("Block %d located in %s, offset address=0x%x\n",
            tmp+1, nand_info->bad_block_info[tmp].device_name, nand_info->bad_block_info[tmp].offset_address); 

        }

            printf("***********************************\n");
        if(refresh_time >= 10)
            printf("Waiting to refresh...\n");

        sleep(refresh_time);
    }
    return ret;
}
