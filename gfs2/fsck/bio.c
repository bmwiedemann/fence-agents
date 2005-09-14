/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2005 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "fsck.h"
#include "bio.h"

/*
 * get_buf - get a buffer
 * @sdp: the super block
 * @blkno: blk # that this buffer will be associated with
 * @bhp: the location where the buffer is returned
 *
 * This function allocates space for a buffer head structure
 * and the corresponding data.  It does not fill in the
 * actual data - that is done by read_buf.
 *
 * Returns: 0 on success, -1 on error
 */
int get_buf(struct fsck_sb *sdp, uint64_t blkno, struct buffer_head **bhp){
	struct buffer_head *bh = NULL;

	*bhp = NULL;
	bh = (struct buffer_head *)malloc(sizeof(struct buffer_head));
	if(!bh){
		log_err("Unable to allocate memory for new buffer head.\n");
		return -1;
	}
	if(!memset(bh, 0, sizeof(struct buffer_head))) {
		log_err("Unable to zero buffer head\n");
		return -1;
	}

	/* FIXME: Not sure how this will work on all
	 * architectures without the casts */
	bh->b_blocknr = blkno;
	bh->b_size = sdp->sb.sb_bsize;
	bh->b_state = 0;
	if(!(bh->b_data = malloc(BH_SIZE(bh)))) {
		free(bh);
		log_err("Unable to allocate memory for new buffer "
			"blkno = %"PRIu64", size = %u\n", blkno, BH_SIZE(bh));
		return -1;
	}
	if(!memset(BH_DATA(bh), 0, BH_SIZE(bh))) {
		free(bh);
		log_err("Unable to zero memory for new buffer "
			"blkno = %"PRIu64", size = %u\n", blkno, BH_SIZE(bh));
	}

	*bhp = bh;

	return 0;
}


/*
 * relse_buf - release a buffer
 * @sdp: the super block
 * @bh: the buffer to release
 *
 * This function will release the memory of the buffer
 * and associated buffer head.
 *
 * Returns: nothing
 */
void relse_buf(struct fsck_sb *sdp, struct buffer_head *bh){
	if(bh){
		if(BH_DATA(bh)) {
			free(BH_DATA(bh));
			bh->b_data = NULL;
		}
		free(bh);
		bh = NULL;
	}
}


/*
 * read_buf - read a buffer
 * @sdp: the super block
 * @blkno: block number
 * @bhp: place where buffer is returned
 * @flags:
 *
 * Returns 0 on success, -1 on error
 */
int read_buf(struct fsck_sb *sdp, struct buffer_head *bh, int flags){
	int disk_fd = sdp->diskfd;

	if(do_lseek(disk_fd, (uint64_t)(BH_BLKNO(bh)*BH_SIZE(bh)))){
		log_err("Unable to seek to position %"PRIu64" "
			"(%"PRIu64" * %u) on storage device.\n",
			(uint64_t)(BH_BLKNO(bh) * BH_SIZE(bh)),
			BH_BLKNO(bh), BH_SIZE(bh));
		return -1;
	}

	if(do_read(disk_fd, BH_DATA(bh), BH_SIZE(bh))){
		log_err("Unable to read %u bytes from position %"PRIu64"\n",
			BH_SIZE(bh), (uint64_t)(BH_BLKNO(bh) * BH_SIZE(bh)));
		return -1;
	}

	return 0;
}


/*
 * write_buf - write a buffer
 * @sdp: the super block
 * @bh: buffer head that describes buffer to write
 * @flags: flags that determine usage
 *
 * Returns: 0 on success, -1 on failure
 */
int write_buf(struct fsck_sb *sdp, struct buffer_head *bh, int flags){
	int disk_fd = sdp->diskfd;

	if(do_lseek(disk_fd, (uint64_t)(BH_BLKNO(bh) * BH_SIZE(bh)))) {
		log_err("Unable to seek to position %"PRIu64
			"(%"PRIu64" * %u) on storage device.\n",
			(uint64_t)(BH_BLKNO(bh) * BH_SIZE(bh)),
			BH_BLKNO(bh), BH_SIZE(bh));
		return -1;
	}

	log_debug("Writing to %"PRIu64" - %"PRIu64" %u\n",
		  (uint64_t)(BH_BLKNO(bh) * BH_SIZE(bh)),
		  BH_BLKNO(bh), BH_SIZE(bh));
	if(do_write(disk_fd, BH_DATA(bh), BH_SIZE(bh))) {
		log_err("Unable to write %u bytes to position %"PRIu64"\n",
			BH_SIZE(bh), (uint64_t)(BH_BLKNO(bh) * BH_SIZE(bh)));
		return -1;
	}

	if(flags & BW_WAIT){
		fsync(disk_fd);
	}

	return 0;
}


/*
 * get_and_read_buf - combines get_buf and read_buf functions
 * @sdp
 * @blkno
 * @bhp
 * @flags
 *
 * Returns: 0 on success, -1 on error
 */
int get_and_read_buf(struct fsck_sb *sdp, uint64_t blkno, struct buffer_head **bhp,
		     int flags)
{
	if(get_buf(sdp, blkno, bhp)) {
		stack;
		return -1;
	}

	if(read_buf(sdp, *bhp, flags)){
		stack;
		relse_buf(sdp, *bhp);
		*bhp = NULL;  /* guarantee that ptr is NULL in failure cases */
		return -1;
	}

	return 0;
}

