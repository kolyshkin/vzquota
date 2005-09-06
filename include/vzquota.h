/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.
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

#include <linux/vzquota.h>
#include <linux/vzctl_quota.h>

#include "kcompat.h"


#define QMAXDEVLEN 128
#define QMAXPATHLEN 128
#define QMAXTYPELEN 32
#define QMAXOPTLEN 128
#define QMAXGARBLEN 512
#define QMAXTIMELEN 64


#ifndef L2

#define BLOCK_SIZE 1024
#define STAT_BLOCK_SIZE 512

#else //L2

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
#define ker2block(x) ((__u32) (((x) + BLOCK_SIZE - 1) >> BLOCK_BITS))

#endif //L2


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
int quotainit_proc(int argc, char **argv);
int quotadrop_proc(int argc, char **argv);

long vzquotactl_syscall(
		int cmd,
		unsigned int quota_id,
		struct vz_quota_stat *qstat,
		const char *ve_root);

#ifdef L2
long vzquotactl_ugid_syscall(
		int _cmd,                /* subcommand */
		unsigned int _quota_id,  /* quota id where it applies to */
		unsigned int _ugid_index,/* for reading statistic. index of first
					    uid/gid record to read */
		unsigned int _ugid_size, /* size of ugid_buf array */
		void *addr               /* user-level buffer */
);
#endif

#define VZCTL_QUOTA_CTL_OLD	_IOWR(VZDQCTLTYPE, 0, struct vzctl_quotactl)

#ifndef L2
struct vz_quota_stat_old  {
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

#define quota_old2new(x)					\
((struct vz_quota_stat) {					\
		bhardlimit	: size_view((x)->bhardlimit),	\
		bsoftlimit	: size_view((x)->bsoftlimit),	\
		bexpire		: (x)->bexpire,			\
		btime		: (x)->btime,			\
		bcurrent	: size_view((x)->bcurrent),	\
								\
		ihardlimit	: (x)->ihardlimit,		\
		isoftlimit	: (x)->isoftlimit,		\
		iexpire		: (x)->iexpire,			\
		itime		: (x)->itime,			\
		icurrent	: (x)->icurrent,		\
								\
		options		: (x)->options			\
	})

#define quota_new2old(x)  						\
((struct vz_quota_stat_old) {						\
		bhardlimit	: block_view((x)->bhardlimit),		\
		bsoftlimit	: block_view((x)->bsoftlimit),		\
		bexpire		: (x)->bexpire,				\
		btime		: (x)->btime,				\
		bcurrent	: block_view((x)->bcurrent),		\
									\
		ihardlimit	: (x)->ihardlimit,			\
		isoftlimit	: (x)->isoftlimit,			\
		iexpire		: (x)->iexpire,				\
		itime		: (x)->itime,				\
		icurrent	: (x)->icurrent,			\
									\
		options		: (x)->options				\
	})

/* L2 */
#else

#define QUOTA_V3	3	/* current 2-level 32-bit 1K quota */
#define QUOTA_V2	2	/* previous 1-level 64-bit byte quota */
#define QUOTA_V1	1	/* first 1-level 32-bit 1K quota */
#define QUOTA_CURRENT	QUOTA_V3

/* converts quota stat between different versions */
void convert_quota_stat( void *dest, int dest_ver, void *src, int src_ver);

/* quota version 2 */
struct vz_quota_stat_old2  {
	/* bytes limits */
	__u64   bhardlimit;     /* absolute limit on disk bytes alloc */
	__u64   bsoftlimit;     /* preferred limit on disk bytes */
	time_t  bexpire;        /* expire timeout for excessive disk use */
	time_t  btime;          /* time limit for excessive disk use */
	__u64   bcurrent;       /* current bytes count */
	/* inodes limits */
	__u32   ihardlimit;     /* absolute limit on allocated inodes */
	__u32   isoftlimit;     /* preferred inode limit */
	__u32   icurrent;       /* current # allocated inodes */
	time_t  iexpire;        /* expire timeout for excessive inode use */
	time_t  itime;          /* time limit for excessive inode use */
	/* behaviour options */
	int     options;        /* see VZ_QUOTA_OPT_* defines */
};

/* quota version 1 */
struct vz_quota_stat_old1  {
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
/* L2 */
#endif


#endif /* __VZQUOTA_UTIL_H__ */
