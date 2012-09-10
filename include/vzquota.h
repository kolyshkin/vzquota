/*
 *  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __VZQUOTA_UTIL_H__
#define __VZQUOTA_UTIL_H__

#define __USE_UNIX98

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <asm/types.h>

#include "linux/vzquota.h"
#include "linux/vzctl_quota.h"

#include "kcompat.h"


#define QMAXDEVLEN 128
#define QMAXPATHLEN 128
#define QMAXTYPELEN 32
#define QMAXOPTLEN 128
#define QMAXGARBLEN 512
#define QMAXTIMELEN 64


/* kernel space data type */
typedef __u64 qint;

/* 1k block size */
#define BLOCK_BITS 10
#define BLOCK_SIZE (1 << BLOCK_BITS)

/* block size used in stat syscall */
#define STAT_BLOCK_BITS 9
#define STAT_BLOCK_SIZE (1 << STAT_BLOCK_BITS)

/* conversion between 1K block and bytes */
#define block2size(x) (((__u64) (x)) << BLOCK_BITS)
#define size2block(x) ((__u32) (((x) + BLOCK_SIZE - 1) >> BLOCK_BITS))

/* conversion between (1K block, bytes) and kernel representation of quota */
/* these macros defines 32-bit or 64-bit quota */
#define size2ker(x) ((qint) (x))
#define block2ker(x) (((qint) (x)) << BLOCK_BITS)
#define ker2size(x) ((__u64) (x))
#define ker2block(x) ((__u64) (((x) + BLOCK_SIZE - 1) >> BLOCK_BITS))

/* defines ration between quota and stat block size */
#define STAT_PER_QUOTA (BLOCK_SIZE / STAT_BLOCK_SIZE)

/* Flags for formatting time */
#define TF_ROUND 0x1		/* Should be printed time rounded? */

/* Maximum retry number */
#define MAX_RETRY 9

extern FILE *mount_fd;
extern char dev[QMAXDEVLEN];

//void usage(char *line);
int vestat_proc(int argc, char **argv);
int quotashow_proc(int argc, char **argv);
int quotaon_proc(int argc, char **argv);
int quotaoff_proc(int argc, char **argv);
int quotaset_proc(int argc, char **argv);
int quotaugidset_proc(int argc, char **argv);
int quotareloadugid_proc(int argc, char **argv);
int quotainit_proc(int argc, char **argv);
int quotadrop_proc(int argc, char **argv);

long vzquotactl_syscall(
		int cmd,
		unsigned int quota_id,
		struct vz_quota_stat *qstat,
		const char *ve_root);

long vzquotactl_ugid_syscall(
		int _cmd,		 /* subcommand */
		unsigned int _quota_id,	 /* quota id where it applies to */
		unsigned int _ugid_index,/* for reading statistic. index of first
					    uid/gid record to read */
		unsigned int _ugid_size, /* size of ugid_buf array */
		void *addr		 /* user-level buffer */
);

#define VZCTL_QUOTA_CTL_OLD	_IOWR(VZDQCTLTYPE, 0, struct vzctl_quotactl)

#define QUOTA_V3	3	/* current 2-level 32-bit 1K quota */
#define QUOTA_V2	2	/* previous 1-level 64-bit byte quota */
#define QUOTA_V1	1	/* first 1-level 32-bit 1K quota */
#define QUOTA_CURRENT	QUOTA_V3

/* converts quota stat between different versions */
void convert_quota_stat( void *dest, int dest_ver, void *src, int src_ver);

/* quota version 2 */
struct vz_quota_stat_old2 {
	/* bytes limits */
	__u64	bhardlimit;	/* absolute limit on disk bytes alloc */
	__u64	bsoftlimit;	/* preferred limit on disk bytes */
	time_t	bexpire;	/* expire timeout for excessive disk use */
	time_t	btime;		/* time limit for excessive disk use */
	__u64	bcurrent;	/* current bytes count */
	/* inodes limits */
	__u32	ihardlimit;	/* absolute limit on allocated inodes */
	__u32	isoftlimit;	/* preferred inode limit */
	__u32	icurrent;	/* current # allocated inodes */
	time_t	iexpire;	/* expire timeout for excessive inode use */
	time_t	itime;		/* time limit for excessive inode use */
	/* behaviour options */
	int	options;	/* see VZ_QUOTA_OPT_* defines */
};

/* quota version 1 */
struct vz_quota_stat_old1 {
	/* bytes limits */
	__u32	bhardlimit;	/* absolute limit on disk 1K blocks alloc */
	__u32	bsoftlimit;	/* preferred limit on disk 1K blocks */
	time_t	bexpire;	/* expire timeout for excessive disk use */
	time_t	btime;		/* time limit for excessive disk use */
	__u32	bcurrent;	/* current 1K blocks count */
	/* inodes limits */
	__u32	ihardlimit;	/* absolute limit on allocated inodes */
	__u32	isoftlimit;	/* preferred inode limit */
	__u32	icurrent;	/* current # allocated inodes */
	time_t	iexpire;	/* expire timeout for excessive inode use */
	time_t	itime;		/* time limit for excessive inode use */
	/* behaviour options */
	int	options;	/* see VZ_QUOTA_OPT_* defines */
};
#endif /* __VZQUOTA_UTIL_H__ */
