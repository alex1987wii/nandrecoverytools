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
#include <mtd/mtd-user.h>
#include "common.h"
#include <libmtd.h>

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

struct mtd_info_user meminfo;
struct mtd_ecc_stats oldstats, newstats;
struct mtd_oob_buf oob;
int fd;


/*
 * Main program
 */
int main(int argc, char **argv)
{
	uint32_t offset = 0;
	int error = 0;
    loff_t address_ofs;



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

    address_ofs= offset;
    printf("Mark block bad at %08lx\n", (long)address_ofs);
    if(ioctl(fd, MEMSETBADBLOCK, &address_ofs) != 0 )
        printf("Mark block fail!\n");

	/* Return happy */
	return 0;
}
