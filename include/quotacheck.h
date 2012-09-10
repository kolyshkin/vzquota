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

#ifndef __QUOTACHECK_H__
#define __QUOTACHECK_H__

#include <sys/types.h>

#include "quota_io.h"

#define HASHSIZE 65536

struct hardlink {
	ino_t inode_num;
	struct hardlink *next;
};

struct dir {
	char *name;
	struct dir *next;
};

struct scan_info {
	/* external fields */
	qint size;
	int inodes;

	/* information (debug) fields */
	int dirs;
	int files;
	int hard_links;

	/* internal fields */
	struct dir *dir_stack;
	struct hardlink *links_hash[HASHSIZE];
	struct ugid_quota *ugid_stat;
	int (*sync_fd)(int fd);
};

void scan(struct scan_info *info, const char *mnt);


#endif /* __QUOTACHECK_H__ */
