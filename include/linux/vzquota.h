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

#ifndef _VZDQUOTA_H
#define _VZDQUOTA_H

#include <linux/types.h>
#include <linux/quota.h>

/* vzquotactl syscall commands */
#define VZ_DQ_CREATE		5 /* create quota master block */
#define VZ_DQ_DESTROY		6 /* destroy qmblk */
#define VZ_DQ_ON		7 /* mark dentry with already created qmblk */
#define VZ_DQ_OFF		8 /* remove mark, don't destroy qmblk */
#define VZ_DQ_SETLIMIT		9 /* set new limits */
#define VZ_DQ_GETSTAT		10 /* get usage statistic */
#define VZ_DQ_OFF_FORCED	11 /* forced off */
/* set of syscalls to maintain UGID quotas */
#define VZ_DQ_UGID_GETSTAT	1 /* get usage/limits for ugid(s) */
#define VZ_DQ_UGID_ADDSTAT	2 /* set usage/limits statistic for ugid(s) */
#define VZ_DQ_UGID_GETGRACE	3 /* get expire times */
#define VZ_DQ_UGID_SETGRACE	4 /* set expire times */
#define VZ_DQ_UGID_GETCONFIG	5 /* get ugid_max limit, cnt, flags of qmblk */
#define VZ_DQ_UGID_SETCONFIG	6 /* set ugid_max limit, flags of qmblk */
#define VZ_DQ_UGID_SETLIMIT	7 /* set ugid B/I limits */
#define VZ_DQ_UGID_SETINFO	8 /* set ugid info */

/* common structure for vz and ugid quota */
struct dq_stat {
	/* blocks limits */
	__u64	bhardlimit;	/* absolute limit in bytes */
	__u64	bsoftlimit;	/* preferred limit in bytes */
	time_t	btime;		/* time limit for excessive disk use */
	__u64	bcurrent;	/* current bytes count */
	/* inodes limits */
	__u32	ihardlimit;	/* absolute limit on allocated inodes */
	__u32	isoftlimit;	/* preferred inode limit */
	time_t	itime;		/* time limit for excessive inode use */
	__u32	icurrent;	/* current # allocated inodes */
};

/* One second resolution for grace times */
#define CURRENT_TIME_SECONDS	(get_seconds())

/* Values for dq_info->flags */
#define VZ_QUOTA_INODES 0x01       /* inodes limit warning printed */
#define VZ_QUOTA_SPACE  0x02       /* space limit warning printed */

struct dq_info {
	time_t		bexpire;   /* expire timeout for excessive disk use */
	time_t		iexpire;   /* expire timeout for excessive inode use */
	unsigned	flags;	   /* see previos defines */
};

struct vz_quota_stat {
	struct dq_stat dq_stat;
	struct dq_info dq_info;
};

/* UID/GID interface record - for user-kernel level exchange */
struct vz_quota_iface {
	unsigned int	qi_id;	   /* UID/GID this applies to */
	unsigned int	qi_type;   /* USRQUOTA|GRPQUOTA */
	struct dq_stat	qi_stat;   /* limits, options, usage stats */
};

/* values for flags and dq_flags */
/* this flag is set if the userspace has been unable to provide usage
 * information about all ugids
 * if the flag is set, we don't allocate new UG quota blocks (their
 * current usage is unknown) or free existing UG quota blocks (not to
 * lose information that this block is ok) */
#define VZDQUG_FIXED_SET	0x01
/* permit to use ugid quota */
#define VZDQUG_ON		0x02
#define VZDQ_USRQUOTA		0x10
#define VZDQ_GRPQUOTA		0x20
#define VZDQ_NOACT		0x1000	/* not actual */
#define VZDQ_NOQUOT		0x2000	/* not under quota tree */

struct vz_quota_ugid_stat {
	unsigned int	limit;	/* max amount of ugid records */
	unsigned int	count;	/* amount of ugid records */
	unsigned int	flags;
};

struct _if_dqblk {
	u_int64_t dqb_bhardlimit;
	u_int64_t dqb_bsoftlimit;
	u_int64_t dqb_curspace;
	u_int64_t dqb_ihardlimit;
	u_int64_t dqb_isoftlimit;
	u_int64_t dqb_curinodes;
	u_int64_t dqb_btime;
	u_int64_t dqb_itime;
	u_int32_t dqb_valid;
};

struct vz_quota_ugid_setlimit {
	unsigned int	type;	/* quota type (USR/GRP) */
	unsigned int	id;	/* ugid */
	struct _if_dqblk dqb;	/* limits info */
};

struct _if_dqinfo {
	u_int64_t dqi_bgrace;
	u_int64_t dqi_igrace;
	u_int32_t dqi_flags;
	u_int32_t dqi_valid;
};

struct vz_quota_ugid_setinfo {
	unsigned int	type;	/* quota type (USR/GRP) */
	struct _if_dqinfo dqi;	/* grace info */
};
#endif /* _VZDQUOTA_H */
