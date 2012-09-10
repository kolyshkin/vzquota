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

#ifndef __VZQUOTA_H__
#define __VZQUOTA_H__

#include "vzquota.h"
#ifndef VARDIR
#define VARDIR		"/var"
#endif
#define VZQUOTA_FILES_PATH_VZ3 VARDIR "/vzquota"
#define VZQUOTA_FILE_NAME "quota"
#define DEFAULT_PRIVATE_FS	"fs"

#define MAGIC_V3	0xFEDCBC27	/* current quota v3 with ugid */
#define MAGIC_V2	0xFEDCBB00	/* quota v2 */
#define MAGIC_V1	0xFEDCBA98	/* quota v1 */
#define MAGIC_CURRENT	MAGIC_V3

#define MAXFILENAMELEN	128	/* Maximal length of filename */

#define E_FILECORRUPT	0x2000	/* Quota file corrupted */

#define QUOTA_ON	0x0001

#define QUOTA_DIRTY	0x0002
#define QUOTA_UGID_ON	0x0004

#define UGID_LOADED	0x0001
#define UGID_DIRTY	0x0002
#define UGID_ALL	0xFFFFFFFF

struct vz_quota_header {
	int magic;
	int flags;
};

#define MAXQUOTAS	2
#define USRQUOTA	0	/* element used for user quotas */
#define GRPQUOTA	1	/* element used for group quotas */

#define DQUOTHASHSIZE	1023	/* Size of hashtable for dquots from file */

#define QUOTANAMES { \
	"user",		/* USRQUOTA */ \
	"group"		/* GRPQUOTA */ \
}

#define NODQUOT ((struct dquot *)NULL)

/* Structure for one loaded quota */
struct ugid_info {
	struct vz_quota_ugid_stat config;	/* flags(on/off, limit_exceeded), ugid_limit,
						   current ugids */
	unsigned int		buf_size;	/* number of used entries in ugid_quota.buf */
	struct dq_info		ugid_info[MAXQUOTAS]; /* array of timeouts */
};

struct ugid_obj {
	struct vz_quota_iface	istat;
	unsigned int		flags; /* set of UGID_OBJ_xxx */
};

struct dquot {
	struct dquot		*dq_next;  /* Pointer to next dquot in the list */
	struct ugid_obj		obj;	   /* Pointer to an element in ugid_quota.buf */
};

/* ugid quota data */
struct ugid_quota {
	struct ugid_info	info;		/* general quota info */
	unsigned int		dquot_size;	/* number of allocated entries in dquot_hash */
	struct dquot		*dquot_hash[MAXQUOTAS][DQUOTHASHSIZE];
};

/* check sum */
typedef __u64 chksum_t;

/* data extracted from quota file */
struct qf_data {
	int			version;	/* field calculated from head.magic */
	struct vz_quota_header	head;		/* quota flags and version */
	struct vz_quota_stat	stat;		/* 1-level quota stat */
	size_t			path_len;	/* mount pount path length */
	char			*path;		/* mount point path */
	struct ugid_quota	ugid_stat;	/* 2-level quota stat and ugid objects */
	chksum_t		chksum;		/* quota file checksum*/
};

#define IOF_ALL		0xFFFFFFFF
#define IOF_HEAD	0x01
#define IOF_STAT	0x02
#define IOF_PATH	0x04
#define IOF_UGID_INFO	0x08
#define IOF_UGID_BUF	0x10
#define IOF_CHKSUM	0x20
#define IOF_UGID_FLAGS	0x40


/* buffer size in bytes for ugid objects read/write operaitons and syscalls */
#ifndef _DEBUG
	#define IO_BUF_SIZE	(64*1024)
#else
	#define IO_BUF_SIZE	(300)
#endif

int open_quota_file(unsigned int quota_id, const char *name, int flags);
int unlink_quota_file(unsigned int quota_id, const char *name);
int close_quota_file(int fd);

int read_field(int fd, void *field, size_t size, off_t offset);
int write_field(int fd, const void *field, size_t size, off_t offset);

int read_quota_file(int fd, struct qf_data *q, int io_flags);
int write_quota_file(int fd, struct qf_data *q, int io_flags);

int check_quota_file(int fd);
int do_check_quota_file(int fd, int reformat);

int get_quota_version(struct vz_quota_header *head);

char *type2name(int type);

/* ugid */

//inline unsigned int hash_dquot(unsigned int id);

struct dquot *lookup_dquot_(struct ugid_quota *q, unsigned int id, unsigned int type);
struct dquot *lookup_dquot(struct ugid_quota *q, struct ugid_obj *obj);

struct dquot *add_dquot_(struct ugid_quota *q, unsigned int id, unsigned int type);
struct dquot *add_dquot(struct ugid_quota *q, struct ugid_obj *obj);

void drop_dquot_(struct ugid_quota *q, unsigned int id, int type);
void drop_dquot(struct ugid_quota *q, struct ugid_obj *obj);

void reset_dquot_search();
struct dquot *get_next_dquot(struct ugid_quota *q);
void sort_dquot(struct ugid_quota *q, struct dquot **obj);

void drop_dummy_ugid(struct ugid_quota *q);
void drop_ugid_by_flags(struct ugid_quota *q, unsigned int mask);

void free_ugid_quota( struct ugid_quota *q);
void clean_ugid_info( struct ugid_quota *q);

void add_ugid_usage(struct ugid_quota *q, unsigned int type, unsigned int id, qint space);
void reset_ugid_usage( struct ugid_quota *q);
void reset_ugid_flags( struct ugid_quota *q, unsigned int mask);

int is_ugid_dirty( struct ugid_quota *q);

int comp_dquot(const void *pa, const void *pb);

/* these func are defined in src/stat.c */
void print_status(struct qf_data *qd);
void print_ugid_status(struct qf_data *qd);

/* quota file */

void init_quota_data(struct qf_data *qd);
void free_quota_data(struct qf_data *qd);

char *get_quota_path(struct qf_data *qd);

//int is_dummy_stat(struct *dq_stat stat);

/* syscalls */
int quota_syscall_on(struct qf_data *qd);
int quota_syscall_off(struct qf_data *qd);
int quota_syscall_stat(struct qf_data *qd, int no_ugid_stat);
int quota_syscall_setlimit(struct qf_data *qd, int no_stat, int no_ugid_stat);

int vzquotactl_ugid_setlimit(struct qf_data * data, int id, int type, struct dq_stat * lim);
int vzquotactl_ugid_setgrace(struct qf_data * data, int type, struct dq_info * lim);

#endif /* __VZQUOTA_H__ */
