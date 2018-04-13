/**
 * f2fs_resize_main.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "f2fs_resize.h"

/*reserve meta area for extend /data to 64G or other*/
#define EXTENT_FOR_256_SECTORS 536870912
#define EXTENT_FOR_128_SECTORS 268435456
#define EXTENT_FOR_64_SECTORS 134217728
#define EXTENT_FOR_32_SECTORS 67108864

#define EXTENT_MAX_SECTORS (EXTENT_FOR_64_SECTORS)

extern struct f2fs_configuration config;

static void res_usage()
{
	MSG(0, "\nUseage: resize [options] device\n");
	MSG(0, "[options]:\n");
	MSG(0, "  -l the size you want to\n");
	exit(1);
}

static void f2fs_prase_options(int argc, char *argv[])
{
    static const char *option_string = "l:d:";
    int32_t option=0;

    while ((option = getopt(argc,argv,option_string)) != EOF) {
	switch (option) {
	case 'l':
	    config.total_sectors = atoll(optarg);
	    break;
		case 'd':
			config.dbg_lv = atoi(optarg);
			break;
		default:
	    MSG(0, "\tError: Unknown option %c\n",option);
	    res_usage();
	    break;
		}
	}
    if (config.total_sectors > EXTENT_MAX_SECTORS ) {
        MSG(-1, "\tINFO: Extent (%lld) larger than reserved (%lld)\n", config.total_sectors, EXTENT_MAX_SECTORS);

        //leave 16KB for encrypting fs
        config.total_sectors = (EXTENT_MAX_SECTORS - 32);
        MSG(-1, "\tINFO: Resize actual total sectors (%lld)\n", config.total_sectors);
    }

    if (optind >= argc) {
	MSG(0, "\tError: Device not specified\n");
	res_usage();
    }
    config.device_name = argv[optind];
}

int f2fs_resize_device()
{
	struct f2fs_sb_info sb_info;
	struct f2fs_sb_info *sbi = &sb_info;
	int ret;

	/* get valid superblock */
	ret = validate_super_block(sbi,0);
	if (ret) {
		ret = validate_super_block(sbi,1);
		if(ret) {
			MSG(0, "\tcat not get valid sb\n");
			return ret;
		}
	}

	/* get valid checkpoint */
	ret = get_valid_checkpoint(sbi);
	if (ret) {
		MSG(0, "\tcat not get valid checkpoint\n");
		return ret;
	}

	/* adjust filesystem super_block */
	ret = f2fs_adjust_super_block(sbi);
	if (ret) {
		MSG(0, "\tadjust super block error\n");
		return ret;
	}

	/* adjust filesystem checkpoint */
	ret = f2fs_adjust_checkpoint(sbi);
	if (ret) {
		MSG(0, "\tadjust checkpoint error\n");
		return ret;
	}

	free(sbi->raw_super);
	free(sbi->ckpt);
	return 0;
}

void f2fs_finalize_dev(struct f2fs_configuration *c)
{
    /*
     * We should call fsync() to flush out all the dirty pages
     * in the block device page cache.
     */
	if (fsync(c->fd) < 0)
		MSG(0, "\tError: Could not conduct fsync!!!\n");

	if (close(c->fd) < 0)
		MSG(0, "\tError: Failed to close device file!!!\n");

}
int main(int argc, char *argv[])
{

	MSG(-1, "Info: begin resize!\n");
	f2fs_init_configuration(&config);

	f2fs_prase_options(argc, argv);

	/* now we only support offline */
	if (f2fs_dev_is_umounted(&config) < 0)
		return -1;

	config.fd = open(config.device_name, O_RDWR);
	if(config.fd < 0) {
		MSG(-1, "\tError: Failed to open the device!\n");
		return -1;
	}

	if(f2fs_resize_device() < 0)
		return -1;

	/* make sure all the change write to device */
	f2fs_finalize_dev(&config);

	MSG(-1, "Info: resize successful!\n");

	return 0;
}
