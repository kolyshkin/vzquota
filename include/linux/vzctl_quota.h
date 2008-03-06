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

#ifndef __LINUX_VZCTL_QUOTA_H__
#define __LINUX_VZCTL_QUOTA_H__

/*
 * Quota management ioctl
 */

struct vz_quota_stat;
struct vzctl_quotactl {
	int cmd;
	unsigned int quota_id;
	struct vz_quota_stat *qstat;
	char *ve_root;
};

struct vzctl_quotaugidctl {
	int cmd;		/* subcommand */
	unsigned int quota_id;	/* quota id where it applies to */
	unsigned int ugid_index;/* for reading statistic. index of first
				    uid/gid record to read */
	unsigned int ugid_size;	/* size of ugid_buf array */
	void *addr; 		/* user-level buffer */
};

#define VZDQCTLTYPE '+'
#define VZCTL_QUOTA_CTL		_IOWR(VZDQCTLTYPE, 1,			\
					struct vzctl_quotactl)
#define VZCTL_QUOTA_NEW_CTL	_IOWR(VZDQCTLTYPE, 2,			\
					struct vzctl_quotactl)
#define VZCTL_QUOTA_UGID_CTL	_IOWR(VZDQCTLTYPE, 3,			\
					struct vzctl_quotaugidctl)

#endif /* __LINUX_VZCTL_QUOTA_H__ */
