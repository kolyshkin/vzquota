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
#ifndef L2
	loff_t size;
#else
	qint size;
#endif
	int inodes;

	/* information (debug) fields */
	int dirs;
	int files;
	int hard_links;

	/* internal fields */
	struct dir *dir_stack;
	struct hardlink *links_hash[HASHSIZE];
#ifdef L2
	struct ugid_quota *ugid_stat;
#endif
};

void scan(struct scan_info *info, const char *mnt);


#endif /* __QUOTACHECK_H__ */
