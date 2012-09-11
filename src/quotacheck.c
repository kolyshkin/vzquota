/*
 *  Copyright (C) 2000-2012, Parallels, Inc. All rights reserved.
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

#include <dirent.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "vzquota.h"
#include "common.h"
#include "quota_io.h"
#include "quotacheck.h"

#ifndef EXT4_SUPER_MAGIC
#define EXT4_SUPER_MAGIC 0xEF53
#endif

/* From ext4.h */
#define EXT4_IOC_ALLOC_DA_BLKS		_IO('f', 12)
static int ext4_sync_fd(int fd)
{
	/* Force block allocation */
	return ioctl(fd, EXT4_IOC_ALLOC_DA_BLKS);
}

static inline struct dir * new_dir_entry(const char * item_name)
{
	struct dir * new_dir = xmalloc(sizeof(struct dir));
	new_dir->name = xstrdup(item_name);
	return new_dir;
}

static inline void insert_dir_entry(struct dir * ind, struct dir * entry)
{
	entry->next = ind->next;
	ind->next = entry;
}

void scan_dir(struct scan_info *info, dev_t root_dev);

/* in bytes! */
static loff_t get_inode_size(struct scan_info *info, const char* fname,
			     struct stat *st)
{
	static int ioctl_fail_warn;
	static int sync_fd_warn;
	int fd;
	loff_t size;

	/* There's no way to do ioctl() on links... */
	if (S_ISLNK(st->st_mode)
	    || !(S_ISDIR(st->st_mode) || S_ISREG(st->st_mode)))
		return ((loff_t) st->st_blocks) << STAT_BLOCK_BITS;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		error(EC_SYSTEM, errno, "Quota check : open '%s'", fname);
	if (info->sync_fd && info->sync_fd(fd) != 0)
		if (!sync_fd_warn)
		{
			sync_fd_warn = 1;
			debug(LOG_WARNING, "Cannot sync file."
			      " Results might be inaccurate.\n");
		}

	if (ioctl(fd, FIOQSIZE, &size) < 0)
	{
		size = ((loff_t) st->st_blocks) << STAT_BLOCK_BITS;
		if (!ioctl_fail_warn)
		{
			ioctl_fail_warn = 1;
			debug(LOG_WARNING, "Cannot get exact used space."
			      " Results might be inaccurate.\n");
		}
	}

	close(fd);
	return size;
}

static inline int hash_index(int inode_num)
{
	return (inode_num & (HASHSIZE - 1));
}

static int check_hard_link(struct scan_info *info, ino_t inode_num)
{
	struct hardlink *hl;
	int hash_id = hash_index(inode_num);

	info->hard_links++;
	for (hl = info->links_hash[hash_id]; hl; hl = hl->next)
		if (hl->inode_num == inode_num)
			return 1;

	hl = (struct hardlink *) xmalloc(sizeof(struct hardlink));
	hl->inode_num = inode_num;
	hl->next = info->links_hash[hash_id];
	info->links_hash[hash_id] = hl;
	return 0;
}


static void add_quota_item(struct scan_info *info, const char * item_name, int resolve_sym, dev_t root_dev)
{
	struct stat st;
	qint qspace;
	loff_t space;

	ASSERT(info && item_name);

	if ((resolve_sym ? stat(item_name, &st) : lstat(item_name, &st)) < 0)
		error(EC_SYSTEM, errno, "quota check : lstat `%s'", item_name);
/*	debug(LOG_DEBUG,
	      "stat %s :st_blocks %d:ino %d:links %d:type %#o\n",
	      item_name, st.st_blocks, st.st_ino, st.st_nlink,
	      st.st_mode & S_IFMT);
*/
	if (st.st_dev != root_dev)
		error(EC_EXDEV, 0, "quota check : crossing mount point");
	if (st.st_nlink > 1 && !S_ISDIR(st.st_mode))
		if (check_hard_link(info, st.st_ino))
			return;

	space = get_inode_size(info, item_name, &st);
/*	debug(LOG_DEBUG, "get size for %s: %llu\n", item_name, space);*/

	qspace = space;
	info->size += qspace;
	info->inodes++;
	if (info->ugid_stat) {
		add_ugid_usage(info->ugid_stat, USRQUOTA, st.st_uid, qspace);
		add_ugid_usage(info->ugid_stat, GRPQUOTA, st.st_gid, qspace);
	}

	if (S_ISDIR(st.st_mode)) {
		// Add new directory entry
		struct dir * new_dir = new_dir_entry(item_name);

		if (info->dir_stack != NULL)
			insert_dir_entry(info->dir_stack, new_dir);
		else {
			new_dir->next = NULL;
			info->dir_stack = new_dir;
		}
/*		debug(LOG_DEBUG, "push dir %s\n", new_dir->name);
*/
		info->dirs++;
	} else {
		info->files++;
	}

	return;
}

void free_lists(struct scan_info *info)
{
	int i;
	struct hardlink *hl, *tmp;

	for (i = 0; i < HASHSIZE; i++) {
		hl = info->links_hash[i];
		while (hl) {
			tmp = hl;
			hl = tmp->next;
			xfree(tmp);
		}
		info->links_hash[i] = NULL;
	}
}

void scan(struct scan_info *info, const char *mnt)
{
	struct stat root_st;
	struct statfs root_stfs;
	struct ugid_quota *ugid_stat = info->ugid_stat;
	int cwd;

	ASSERT(info && mnt);

	debug(LOG_INFO, "scan mount point begin '%s'\n", mnt);

	if ((cwd = open(".", O_RDONLY)) == -1)
		error(0, errno, "failed to open cwd");

	memset(info, 0, sizeof(struct scan_info));
	if (ugid_stat) {
		info->ugid_stat = ugid_stat;
		reset_ugid_usage(info->ugid_stat);
	}
	if (stat(mnt, &root_st) == -1)
		error(EC_SYSTEM, errno, "quota check : stat %s", mnt);

	if (!S_ISDIR(root_st.st_mode))
		error(EC_SYSTEM, errno, "quota check : path %s is not dir",
				mnt);
	if (statfs(mnt, &root_stfs) == -1)
		error(EC_SYSTEM, errno, "quota check : statfs %s", mnt);
	if (root_stfs.f_type == EXT4_SUPER_MAGIC) {
		DIR *dp = opendir(mnt);
		int dfd;
		if (dp == (DIR *) NULL)
			error(EC_SYSTEM, errno, "quota check : opendir '%s'", mnt);
		dfd = dirfd(dp);
		if (dfd < 0)
			error(EC_SYSTEM, errno, "quota check : dirfd '%s'", mnt);
		if (ext4_sync_fd(dfd) != -1)
			info->sync_fd = ext4_sync_fd;
		closedir(dp);
	}
	add_quota_item(info, mnt, 1, root_st.st_dev);

	while (info->dir_stack != (struct dir *) NULL) {
		struct dir *next_dir;

		// scan dir on top of info->dir_stack
		scan_dir(info, root_st.st_dev);
		next_dir = info->dir_stack->next;

		xfree(info->dir_stack->name);
		xfree(info->dir_stack);

		info->dir_stack = next_dir;
	}
	if (info->ugid_stat) {
		/* drop dummy ugid entries */
		drop_dummy_ugid(info->ugid_stat);

		/* update size of buffer */
		info->ugid_stat->info.buf_size = info->ugid_stat->dquot_size;
	}
	if (cwd != -1) {
		if (fchdir(cwd))
			error(0, errno, "failed to fchdir cwd");
		close(cwd);
	}

	free_lists(info);

	debug(LOG_INFO,
	      "scan mount point end :%s:inodes %d:size %lld:files %d:dirs %d:hard_links %d\n",
	      mnt, info->inodes, info->size, info->files,
	      info->dirs, info->hard_links);
	return;
}

void scan_dir(struct scan_info *info, dev_t root_dev)
{
	DIR *dp;
	struct dirent *de;
	const char *pathname = info->dir_stack->name;

	if (chdir(pathname) != 0)
		error(EC_SYSTEM, errno, "quota check : chdir '%s'", pathname);

	// check on special case
	if (!strcmp(pathname, ".."))
		return ;
/*	debug(LOG_DEBUG, "scan dir begin :%s:\n", pathname);
*/
	// add return path
	insert_dir_entry(info->dir_stack, new_dir_entry(".."));

	dp = opendir(".");
	if (dp == (DIR *) NULL)
		error(EC_SYSTEM, errno, "quota check : opendir '%s'", pathname);

	while ((de = readdir(dp)) != (struct dirent *) NULL) {
		const char * p = de->d_name;
		// skip '.', '..'
		if (p[0] == '.' && (p[1] == '\0'
				    || (p[1] == '.' && p[2] == '\0')))
			continue;
		add_quota_item(info, de->d_name, 0, root_dev);
	}

	closedir(dp);

/*	debug(LOG_DEBUG, "scan dir end :%s:\n", pathname);
*/
}


