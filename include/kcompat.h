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

#ifndef __KCOMPAT_H__
#define __KCOMPAT_H__

/*
 * This header file defines kernel structures for old quota interfaces
 */
#ifndef Q_SETGRACE

#define Q_SETGRACE	0x0B00	/* set inode and block grace */
#define Q_SETQLIM	0x0700	/* set limits */


/* This is in-memory copy of quota block. See meaning of entries above */
struct mem_dqblk {
	unsigned int dqb_ihardlimit;
	unsigned int dqb_isoftlimit;
	unsigned int dqb_curinodes;
	unsigned int dqb_bhardlimit;
	unsigned int dqb_bsoftlimit;
	__u64 dqb_curspace;
	__kernel_time_t dqb_btime;
	__kernel_time_t dqb_itime;
};

/* Inmemory copy of version specific information */
struct mem_dqinfo {
	unsigned int dqi_bgrace;
	unsigned int dqi_igrace;
	unsigned int dqi_flags;
	unsigned int dqi_blocks;
	unsigned int dqi_free_blk;
	unsigned int dqi_free_entry;
};

#endif

#endif /* __KCOMPAT_H__ */
