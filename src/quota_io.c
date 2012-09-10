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

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <libgen.h>

#include "common.h"
#include "vzquota.h"
#include "quota_io.h"


#define QF_OFF_HEADER		0
#define QF_OFF_STAT		(QF_OFF_HEADER + sizeof(struct vz_quota_header))
#define QF_OFF_PATH_LEN(size)	(QF_OFF_STAT + (size))
#define QF_OFF_PATH(size)	(QF_OFF_PATH_LEN(size) + sizeof(size_t))

#define QF_OFF_UGID_INFO(path_len)	(QF_OFF_PATH(sizeof(struct vz_quota_stat)) + (path_len))
#define QF_OFF_UGID_BUF(path_len)	(QF_OFF_UGID_INFO(path_len) + sizeof(struct ugid_info))

#define min(a,b)	(((a) > (b)) ? (b) : (a))

static inline int is_dummy_stat(struct dq_stat *stat)
{
	return !(stat->bhardlimit || stat->bsoftlimit || stat->bcurrent ||
		stat->ihardlimit || stat->isoftlimit || stat->icurrent);
}

static char quotanames[MAXQUOTAS + 1][20] = QUOTANAMES;

/* Convert type of quota to written representation */
char *type2name(int type)
{
	return quotanames[type];
}

/* Hash given id */
static inline unsigned int hash_dquot(unsigned int id)
{
	return ((id ^ (id << 16)) * 997) & (DQUOTHASHSIZE - 1);
}

/* Do a lookup of a type of quota for a specific id. Use short cut with
 * most recently used dquot struct pointer. */
struct dquot *lookup_dquot_(struct ugid_quota *q, unsigned int id, unsigned int type)
{
	struct dquot *lptr;
	unsigned int hash = hash_dquot(id);

	for (lptr = q->dquot_hash[type][hash]; lptr != NODQUOT; lptr = lptr->dq_next)
		if (lptr->obj.istat.qi_id == id)
			return lptr;
	return NODQUOT;
}

struct dquot *lookup_dquot(struct ugid_quota *q, struct ugid_obj *obj)
{
	return lookup_dquot_(q, obj->istat.qi_id, obj->istat.qi_type);
}

/* Add a new dquot for a new id to the list. */
struct dquot *add_dquot_(struct ugid_quota *q, unsigned int id, unsigned int type)
{
	struct dquot *lptr;
	unsigned int hash = hash_dquot(id);

	lptr = (struct dquot *)xmalloc(sizeof(struct dquot));

	lptr->obj.istat.qi_id = id;
	lptr->obj.istat.qi_type = type;
	lptr->dq_next = q->dquot_hash[type][hash];
	q->dquot_hash[type][hash] = lptr;

	q->dquot_size++;
/*	debug(LOG_DEBUG, "add dquot for ugid(id=%u,type=%u)\n",
		lptr->obj.istat.qi_id, lptr->obj.istat.qi_type);
*/
	return lptr;
}

struct dquot *add_dquot(struct ugid_quota *q, struct ugid_obj *obj)
{
	struct dquot *lptr;

	lptr = add_dquot_(q, obj->istat.qi_id, obj->istat.qi_type);
	memcpy(&(lptr->obj), obj, sizeof(struct ugid_obj));

	return lptr;
}

/* Drop dquot from the list. */
void drop_dquot_(struct ugid_quota *q, unsigned int id, int type)
{
	struct dquot *lptr, *prev;
	unsigned int hash = hash_dquot(id);

	prev = NODQUOT;
	for (lptr = q->dquot_hash[type][hash]; lptr != NODQUOT; prev = lptr, lptr = lptr->dq_next) {
		if (lptr == NODQUOT) return;
		if (lptr->obj.istat.qi_id == id) {
			if (prev == NODQUOT) {
				q->dquot_hash[type][hash] = lptr->dq_next;
			} else {
				prev->dq_next = lptr->dq_next;
			}
			debug(LOG_DEBUG, "drop ugid (%s %u)\n",
				type2name(lptr->obj.istat.qi_type), lptr->obj.istat.qi_id);
			free(lptr);
			if (q->dquot_size > 0) q->dquot_size--;
			return;
		}
	}
}

void drop_dquot(struct ugid_quota *q, struct ugid_obj *obj)
{
	drop_dquot_(q, obj->istat.qi_id, obj->istat.qi_type);
	return;
}

/* compare dquot */
int comp_dquot(const void *pa, const void *pb)
{
	struct dquot *a = *(struct dquot **)pa;
	struct dquot *b = *(struct dquot **)pb;

	return (a->obj.istat.qi_id > b->obj.istat.qi_id) ? 1 :
		((a->obj.istat.qi_id < b->obj.istat.qi_id) ? -1 : 0);
}

/* search facility */
unsigned int	cur_index;
unsigned int	cur_id;
unsigned int	cur_type;
struct dquot	*cur_dquot;

void reset_dquot_search()
{
	cur_index = 0;
	cur_id = 0;
	cur_type = 0;
	cur_dquot = NODQUOT;
}

struct dquot *get_next_dquot(struct ugid_quota *q)
{
	if (cur_index < q->dquot_size)
		for (; cur_type < MAXQUOTAS; cur_type++) {
			for (; cur_id < DQUOTHASHSIZE; cur_id++) {
				if (cur_dquot != NODQUOT)
					cur_dquot = cur_dquot->dq_next;
				else
					cur_dquot = q->dquot_hash[cur_type][cur_id];
				if (cur_dquot != NODQUOT) {
					cur_index++;
					return cur_dquot;
				}
			}
			if (cur_id >= DQUOTHASHSIZE) cur_id = 0;
		}
	reset_dquot_search();
	return NODQUOT;
}

/* sort ugids objects by ID;
 * obj should be an allocated array of (struct dquot *) of dquot_size */
void sort_dquot(struct ugid_quota *q, struct dquot **obj)
{
	unsigned int i;
	struct dquot *dq;

	reset_dquot_search();
	for (i = 0; (dq = get_next_dquot(q)) != NODQUOT; i++)
		obj[i] = dq;
	qsort(obj, q->dquot_size, sizeof(struct dquot *), &comp_dquot);
}

/* drop dummy entries */
void drop_dummy_ugid(struct ugid_quota *q)
{
	struct dquot *dquot;
	void **buf = NULL;
	size_t i, n = 0, size = 0;

	/* select ugids */
	reset_dquot_search();
	while( (dquot = get_next_dquot(q)) != NODQUOT) {
		if (!is_dummy_stat(&(dquot->obj.istat.qi_stat))) continue;
		if (n >= size) {
			size += 100;
			buf = xrealloc(buf, size * sizeof(void *));
		}
		buf[n] = (void *) dquot;
		n++;
	}
	/* drop ugids */
	for (i = 0; i < n; i++) {
		dquot = (struct dquot *) buf[i];
		drop_dquot(q, &(dquot->obj));
	}
	free(buf);
}

/* drop ugid entries by flags */
void drop_ugid_by_flags(struct ugid_quota *q, unsigned int mask)
{
	struct dquot *dquot;
	void **buf = NULL;
	size_t i, n = 0, size = 0;

	/* select ugids */
	reset_dquot_search();
	while( (dquot = get_next_dquot(q)) != NODQUOT) {
		if (!(dquot->obj.flags & mask)) continue;
		if (n >= size) {
			size += 100;
			buf = xrealloc(buf, size * sizeof(void *));
		}
		buf[n] = (void *) dquot;
		n++;
	}
	/* drop ugids */
	for (i = 0; i < n; i++) {
		dquot = (struct dquot *) buf[i];
		drop_dquot(q, &(dquot->obj));
	}
}

/* clean dquots memory */
void free_ugid_quota(struct ugid_quota *q)
{
	unsigned int i, cnt;
	struct dquot *dquot, *dquot_free;

	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		for (i = 0; i < DQUOTHASHSIZE; i++) {
			dquot = q->dquot_hash[cnt][i];
			while (dquot != NODQUOT) {
				dquot_free = dquot;
				dquot = dquot->dq_next;
				free(dquot_free);
			}
			q->dquot_hash[cnt][i] = NODQUOT;
		}
	}
	q->dquot_size = 0;
}

/* clean ugid info */
void clean_ugid_info( struct ugid_quota *q)
{
	memset( &(q->info), 0, sizeof(struct ugid_info));
}


/* Add a number of blocks and inodes to a quota */
void add_ugid_usage(struct ugid_quota *q, unsigned int type, unsigned int id, qint space)
{
	struct dquot *lptr;
/*	__u32 i;
	qint b;
*/

	if ((lptr = lookup_dquot_(q, id, type)) == NODQUOT)
		lptr = add_dquot_(q, id, type);

/*	i = lptr->obj.istat.qi_stat.icurrent;
	b = lptr->obj.istat.qi_stat.bcurrent;
*/

	lptr->obj.istat.qi_stat.icurrent++;
	lptr->obj.istat.qi_stat.bcurrent += space;

/*	debug(LOG_DEBUG, "add %llu bytes to ugid(%u,%s): "
		"(i=%u,b:%llu) -> (i=%u,b=%llu)\n", space,
		lptr->obj.istat.qi_id, type2name(lptr->obj.istat.qi_type),
		i, b,
		lptr->obj.istat.qi_stat.icurrent, lptr->obj.istat.qi_stat.bcurrent);
*/
}

/* reset usage stat of ugid objects */
void reset_ugid_usage( struct ugid_quota *q)
{
	struct dquot *dq;

	reset_dquot_search();
	while ((dq = get_next_dquot(q)) != NODQUOT) {
		dq->obj.istat.qi_stat.bcurrent = 0;
		dq->obj.istat.qi_stat.icurrent = 0;
	}
}

/* reset flags of ugid objects */
void reset_ugid_flags( struct ugid_quota *q, unsigned int mask)
{
	struct dquot *dquot;

	reset_dquot_search();
	while ((dquot = get_next_dquot(q)) != NODQUOT) {
		dquot->obj.flags &= ~mask;
	}

}

/* are there dirty ugid objects */
int is_ugid_dirty( struct ugid_quota *q)
{
	struct dquot *dquot;

	reset_dquot_search();
	while ((dquot = get_next_dquot(q)) != NODQUOT) {
		if (dquot->obj.flags & UGID_DIRTY) return 1;
	}
	return 0;
}

/* quota file data init; it should be called as A MUST */
void init_quota_data(struct qf_data *qd)
{
	qd->version = 0;
	memset(&(qd->head), 0, sizeof(struct vz_quota_header));
	memset(&(qd->stat), 0, sizeof(struct vz_quota_stat));
	qd->path_len = 0;
	qd->path = NULL;
	memset(&qd->chksum, 0, sizeof(qd->chksum));

	clean_ugid_info(&(qd->ugid_stat));
	qd->ugid_stat.dquot_size = 0;
	memset(qd->ugid_stat.dquot_hash, 0,
		MAXQUOTAS * DQUOTHASHSIZE * sizeof(struct dquot *));
}

/* quota file data cleaning */
void free_quota_data(struct qf_data *qd)
{
	free_ugid_quota(&(qd->ugid_stat));
	if (qd->path) free(qd->path);
	qd->path = NULL;

	init_quota_data(qd);
}

/* Get quota accounting point path */
char *get_quota_path(struct qf_data *qd)
{
	char *path;

	path = NULL;
	if (mount_point)
		path = mount_point;
	else if (qd->path)
		path = qd->path;

	if (path)
		debug(LOG_DEBUG, "Actual quota path: %s\n", path);
	else
		error(EC_NOMOUNTPOINT, 0, "path to mount point can't be read "
			"from config file,\nuse -p <mount_point> option\n");
	return path;
}

/* converts quota between different versions */
void convert_quota_stat( void *dest, int dest_ver, void *src, int src_ver)
{
	struct vz_quota_stat *q;
	struct vz_quota_stat_old1 *q1;
	struct vz_quota_stat_old2 *q2;

	if ( (!dest) || (!src)
		|| (!((dest_ver == QUOTA_V3) || (dest_ver == QUOTA_V2) || (dest_ver == QUOTA_V1)))
		|| (!((src_ver == QUOTA_V3) || (src_ver == QUOTA_V2) || (src_ver == QUOTA_V1)))
	   ) return;

	if (dest_ver == QUOTA_V1) {
		if (src_ver == QUOTA_V1) {			/* v1 -> v1 */
			memcpy( dest, src, sizeof(struct vz_quota_stat_old1));

		} else if (src_ver == QUOTA_V2) {		/* v2 -> v1 */
			q1 = (struct vz_quota_stat_old1 *) dest;
			q2 = (struct vz_quota_stat_old2 *) src;

			q1->bhardlimit	= size2block(q2->bhardlimit);
			q1->bsoftlimit	= size2block(q2->bsoftlimit);
			q1->bexpire	= q2->bexpire;
			q1->btime	= q2->btime;
			q1->bcurrent	= size2block(q2->bcurrent);

			q1->ihardlimit	= q2->ihardlimit;
			q1->isoftlimit	= q2->isoftlimit;
			q1->iexpire	= q2->iexpire;
			q1->itime	= q2->itime;
			q1->icurrent	= q2->icurrent;

			q1->options	= q2->options;

		} else if (src_ver == QUOTA_V3) {		/* v3 -> v1 */
			struct vz_quota_stat_old2 t;
			/* v3 -> v2 -> v1 */
			convert_quota_stat( &t, QUOTA_V2, src, QUOTA_V3);
			convert_quota_stat( dest, QUOTA_V1, &t, QUOTA_V2);
		}

	} else if (dest_ver == QUOTA_V2) {
		if (src_ver == QUOTA_V1) {			/* v1 -> v2 */
			q1 = (struct vz_quota_stat_old1 *) src;
			q2 = (struct vz_quota_stat_old2 *) dest;

			q2->bhardlimit	= block2size(q1->bhardlimit);
			q2->bsoftlimit	= block2size(q1->bsoftlimit);
			q2->bexpire	= q1->bexpire;
			q2->btime	= q1->btime;
			q2->bcurrent	= block2size(q1->bcurrent);

			q2->ihardlimit	= q1->ihardlimit;
			q2->isoftlimit	= q1->isoftlimit;
			q2->iexpire	= q1->iexpire;
			q2->itime	= q1->itime;
			q2->icurrent	= q1->icurrent;

			q2->options	= q1->options;


		} else if (src_ver == QUOTA_V2) {		/* v2 -> v2 */
			memcpy( dest, src, sizeof(struct vz_quota_stat_old2));

		} else if (src_ver == QUOTA_V3) {		/* v3 -> v2 */
			q2 = (struct vz_quota_stat_old2 *) dest;
			q = (struct vz_quota_stat *) src;

			q2->bhardlimit	= ker2size(q->dq_stat.bhardlimit);
			q2->bsoftlimit	= ker2size(q->dq_stat.bsoftlimit);
			q2->bexpire	= q->dq_info.bexpire;
			q2->btime	= q->dq_stat.btime;
			q2->bcurrent	= ker2size(q->dq_stat.bcurrent);

			q2->ihardlimit	= q->dq_stat.ihardlimit;
			q2->isoftlimit	= q->dq_stat.isoftlimit;
			q2->iexpire	= q->dq_info.iexpire;
			q2->itime	= q->dq_stat.itime;
			q2->icurrent	= q->dq_stat.icurrent;

			q2->options	= 0;
		}

	} else if (dest_ver == QUOTA_V3) {
		if (src_ver == QUOTA_V1) {			/* v1 -> v3 */
			struct vz_quota_stat_old2 t;
			/* v1 -> v2 -> v3 */
			convert_quota_stat( &t, QUOTA_V2, src, QUOTA_V1);
			convert_quota_stat( dest, QUOTA_V3, &t, QUOTA_V2);

		} else if (src_ver == QUOTA_V2) {		/* v2 -> v3 */
			q2 = (struct vz_quota_stat_old2 *) src;
			q = (struct vz_quota_stat *) dest;

			q->dq_stat.bhardlimit	= size2ker(q2->bhardlimit);
			q->dq_stat.bsoftlimit	= size2ker(q2->bsoftlimit);
			q->dq_info.bexpire	= q2->bexpire;
			q->dq_stat.btime	= q2->btime;
			q->dq_stat.bcurrent	= size2ker(q2->bcurrent);

			q->dq_stat.ihardlimit	= q2->ihardlimit;
			q->dq_stat.isoftlimit	= q2->isoftlimit;
			q->dq_info.iexpire	= q2->iexpire;
			q->dq_stat.itime	= q2->itime;
			q->dq_stat.icurrent	= q2->icurrent;

			q->dq_info.flags	= 0;

		} else if (src_ver == QUOTA_V3) {		/* v3 -> v3 */
			memcpy( dest, src, sizeof(struct vz_quota_stat));
		}
	}
}

/*
 * Show hints and error message in case of quota on failed.
 */
static void error_quotaon(struct qf_data *qd, int err, char *buf)
{
	/* show error message */
	error(0, err, "Quota on syscall for id %d", quota_id);

	/* try to find the reason */
	if (err == EBUSY) {
		error(0, 0, "\tPossible reasons:");
		error(0, 0, "\t- Container's root is already mounted");
		error(0, 0, "\t- there are opened files inside Container's private"
				" area");
		error(0, 0, "\t- your current working directory is inside Container's");
		error(0, 0, "\t  private area");
		if (buf) {
			error(0, 0, "\tCurrently used file(s):\n%s", buf);
			if (strlen(buf) > getpagesize() - 2)
				error(0, 0, "\t... more files found");
		}
	} else if (err == EEXIST) {
		error(0, 0, "\tNative quota is already running "
				"for this partition.");
	}

	/* exit with error status */
	error(EC_VZCALL, 0, NULL);
}

/* turn quota on
 * return 0 if success,
 * <0 if quota is on */
int quota_syscall_on(struct qf_data *qd)
{
	int rc;
	int retry = 0;
	int sleeptime = 1;
	struct vz_quota_stat *stat;
	struct ugid_quota *ugid_stat;
	char *path;
	char *buf;

	ASSERT(qd);
	stat = &qd->stat;
	ugid_stat = &qd->ugid_stat;
	path = get_quota_path(qd);

	/* create new quota */
	rc = vzquotactl_syscall(VZ_DQ_CREATE, quota_id, stat, NULL);
	if (rc < 0) {
		if (errno == EEXIST)
			return rc;
		else
			error(EC_VZCALL, errno, "Quota create for id %d", quota_id);
	}

	/* null ugid config */
	ugid_stat->info.config.flags = 0;
	ugid_stat->info.config.count = 0;

	/* set kernel flag of user/group quota status */
	if (qd->head.flags & QUOTA_UGID_ON)
		ugid_stat->info.config.flags |= VZDQUG_ON;

	/* set ugid config */
	if ((ugid_stat->info.config.flags & VZDQUG_ON)
			&& !ugid_stat->info.config.limit)
		/* set indicator of ugids are broken if quota is on and limit=0 */
		ugid_stat->info.config.flags |= VZDQUG_FIXED_SET;

	rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_SETCONFIG, quota_id, 0, 0,
			&(ugid_stat->info.config));
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota ugid_setconfig syscall for id %d", quota_id);

	/* set ugid graces and flags */
	if (ugid_stat->info.config.flags & VZDQUG_ON) {
		rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_SETGRACE, quota_id, 0, 0,
				&(ugid_stat->info.ugid_info));
		if (rc < 0)
			error(EC_VZCALL, errno, "Quota ugid_setgrace syscall for id %d", quota_id);
	}

	/* add ugid objects */
	if ((ugid_stat->info.config.flags & VZDQUG_ON)
			&& ugid_stat->dquot_size
			&& ugid_stat->info.config.limit) {

		struct dquot *obj[ugid_stat->dquot_size];
		struct dquot *dq;
		unsigned int b_size, loaded, i, lim;
		struct vz_quota_iface *buf;

		/* drop dummy entries */
		drop_dummy_ugid(ugid_stat);

		/* create temp buf */
		reset_dquot_search();
		for (i = 0; (dq = get_next_dquot(ugid_stat)) != NODQUOT; i++)
			obj[i] = dq;

		/* sort if number of ugid objects is over the limit */
		if (ugid_stat->dquot_size > ugid_stat->info.config.limit)
			qsort(obj, ugid_stat->dquot_size, sizeof(struct dquot *), &comp_dquot);

		/* load by portions */
		lim = min(ugid_stat->dquot_size, ugid_stat->info.config.limit);
		b_size = IO_BUF_SIZE / sizeof(struct vz_quota_iface);
		b_size = min(b_size, lim);
		buf = xmalloc(b_size * sizeof(struct vz_quota_iface));
		loaded = 0;

		for (i = 0; i < lim; i += b_size) {
			unsigned int j;
			/* fill buf */
			for (j = i; j < min(lim, i + b_size); j++) {
				dq = obj[j];
				memcpy( &buf[j-i], &(dq->obj.istat), sizeof(struct vz_quota_iface));
			}
			/* push buf to kernel */
			debug(LOG_DEBUG, "push ugids from %u to %u...\n", i, j - 1);
			rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_ADDSTAT, quota_id, 0, j - i, buf);
			if (rc < 0) {
				free(buf);
				error(EC_VZCALL, errno, "Quota ugid_addstat syscall for id %d", quota_id);
			}
			debug(LOG_DEBUG, "%u ugids were successfully pushed\n", rc);
			for (j = 0; j < (unsigned)rc; j++)
				debug(LOG_DEBUG,"%5d: ugid (%s %u) was pushed\n",
					i + j, type2name(buf[j].qi_type), buf[j].qi_id);
			loaded += rc;
		}
		free(buf);

		/* indicate to kernel that ugid limit is exceeded */
		if (loaded < ugid_stat->dquot_size) {
			debug(LOG_WARNING, "Quotas were not loaded for some users/groups for id "
					"%d due to ugid limit\n", quota_id);
			ugid_stat->info.config.flags |= VZDQUG_FIXED_SET;
			rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_SETCONFIG, quota_id, 0, 0,
				&(ugid_stat->info.config));
			if (rc < 0)
			error(EC_VZCALL, errno, "Quota ugid_setconfig syscall for id %d", quota_id);
		}

		/* mark loaded entries */
		for (i = 0; i < loaded; i++) {
			dq = obj[i];
			dq->obj.flags |= UGID_LOADED;
		}
		ugid_stat->info.config.count = loaded;

		/* mark unloaded entries as dirty */
		for (i = loaded; i < ugid_stat->dquot_size; i++) {
			dq = obj[i];
			dq->obj.flags |= UGID_DIRTY;
		}

		/* set number of ugid objects in buffer */
		ugid_stat->info.buf_size = ugid_stat->dquot_size;
	}

	buf = NULL;

	/* turn quota on */
	for (retry = 0; ; retry++) {
		 /*
		  * If this is the last chance to enable quota, allocate memory buffer
		  * and ask kernel to find currently used files.
		  */
		if (retry == MAX_RETRY) {
			buf = (char *)malloc(getpagesize());
			*buf = '\0';
		}
		rc = vzquotactl_syscall(VZ_DQ_ON, quota_id,
					(struct vz_quota_stat *)buf, path);
		if (rc >= 0) break;
		if (errno != EBUSY || retry >= MAX_RETRY) {
			int save_errno;

			save_errno = errno;
			vzquotactl_syscall(VZ_DQ_DESTROY, quota_id, NULL, NULL);
			error_quotaon(qd, save_errno, buf);
		}
		usleep(sleeptime * 1000);
		sleeptime = sleeptime * 2;
	}

	if (ugid_stat->info.config.flags & VZDQUG_ON)
		debug(LOG_INFO, "User/group quota was activated with ugid limit of %u\n",
				ugid_stat->info.config.limit);
	else
		debug(LOG_INFO, "User/group quota is off\n");

	return 0;
}

/*
 * Show hints and error message in case of quota off failed.
 */
static void error_quotaoff(struct qf_data *qd, int err, char *buf)
{
	/* show error message */
	error(0, err, "Quota off syscall for id %d", quota_id);

	/* try to find the reason */
	if (err == EBUSY && (option & FL_FORCE))
		/* Hmm, shouldn't be */
		error(0, 0, "Forced quota off failed. Device is busy\n");
	else if (err == EBUSY) {
		error(0, 0, "\tPossible reasons:");
		error(0, 0, "\t- Container's root is not unmounted");
		error(0, 0, "\t- there are opened files inside container's root/private"
				" area");
		error(0, 0, "\t- your current working directory is inside a container directory");
		error(0, 0, "\t  root/private area");
		if (buf) {
			error(0, 0, "\tCurrently used file(s):\n%s", buf);
			if (strlen(buf) > getpagesize() - 2)
				error(0, 0, "\t... more files found");
		}
	} else if (err == EINVAL && (option & FL_FORCE)) {
		error(0, 0, "\tPossible reasons:");
		error(0, 0, "\t- Kernel doesn't support forced quota off");
	}

	/* exit with error status */
	error(EC_VZCALL, 0, NULL);

}

/* turn quota off
 * return 0 if success,
 * OENT if quota is off */
int quota_syscall_off(struct qf_data *qd)
{
	int rc;
	int retry = 0;
	int sleeptime = 1;
	int broken = 0;
	struct vz_quota_stat *stat;
	struct ugid_quota *ugid_stat;
	int cmd;
	char *buf;

	ASSERT(qd);
	stat = &qd->stat;
	ugid_stat = &qd->ugid_stat;

	cmd = VZ_DQ_OFF;
	buf = NULL;

	/* turn quota off */
	for (retry = 0; ; retry++) {
		/*
		 * If this is the last chance to disable quota, allocate memory buffer
		 * and ask kernel to find currently used files.
		 */
		if (retry == MAX_RETRY && debug_level >= LOG_WARNING) {
			buf = (char *)malloc(getpagesize());
			*buf = '\0';
		}
		rc = vzquotactl_syscall(cmd, quota_id, NULL, buf);
		if (rc >= 0) break;
		if (errno == EALREADY || errno == EIO) {/* quota is created and off */
			broken = errno;
			goto destroy;
		}
		if (errno == ENOENT) return rc;		/* quota is not created */
		if (errno != EBUSY || retry >= MAX_RETRY)
			error_quotaoff(qd, errno, buf);
		usleep(sleeptime * 1000);
		sleeptime = sleeptime * 2;

		/*
		 * When 'force' option is set, try to use VZ_DQ_OFF first,
		 * and in case of failure use VZ_DQ_OFF_FORCED.
		 */
		if (errno == EBUSY && (option & FL_FORCE) &&
				(retry >= (MAX_RETRY - 1))) {
			cmd = VZ_DQ_OFF_FORCED;
			/* Mark as dirty now */
			qd->head.flags |= QUOTA_DIRTY;
		}
	}

	/* get VE stat */
	rc = vzquotactl_syscall(VZ_DQ_GETSTAT, quota_id, stat, NULL);
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota getstat syscall for id %d", quota_id);

	/* get ugid config */
	rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_GETCONFIG, quota_id, 0, 0,
			&(ugid_stat->info.config));
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota ugid_getconfig syscall for id %d", quota_id);

	/* get ugid graces */
	if (ugid_stat->info.config.flags & VZDQUG_ON) {
		rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_GETGRACE, quota_id, 0, 0,
				&(ugid_stat->info.ugid_info));
		if (rc < 0)
			error(EC_VZCALL, errno, "Quota ugid_getgrace syscall for id %d", quota_id);
	}

	/* get ugid objects */
	if ((ugid_stat->info.config.flags & VZDQUG_ON)
/* TODO save loaded objs if limit was set to 0 ?
 *			&& ugid_stat->info.config.limit
*/
			&& ugid_stat->info.config.count) {

		struct dquot *dq;
		unsigned int b_size, i;
		struct vz_quota_iface *buf;

		/* get by portions */
		b_size = IO_BUF_SIZE / sizeof(struct vz_quota_iface);
		b_size = min(b_size, ugid_stat->info.config.count);

		buf = xmalloc(b_size * sizeof(struct vz_quota_iface));

		for (i = 0; i < ugid_stat->info.config.count; i += b_size) {
			int j;
			j = min(b_size, ugid_stat->info.config.count - i);
			debug(LOG_DEBUG, "get ugids from %u to %u...\n", i, i + j - 1);
			rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_GETSTAT, quota_id, i, j, buf);
			if (rc < 0) {
				free(buf);
				error(EC_VZCALL, errno, "Quota ugid_getstat syscall for id %d", quota_id);
			}
			debug(LOG_DEBUG, "%u ugids were received\n", rc);
			for (j = 0; j < rc; j++) {
				if (buf[j].qi_type >= MAXQUOTAS)
					error(EC_VZCALL, 0, "Quota ugid_getstat syscall for id %d returned "
						"ugid object with invalid type (id=%u, type=%u)",
						quota_id, buf[j].qi_id, buf[j].qi_type);

				dq = lookup_dquot_(ugid_stat, buf[j].qi_id, buf[j].qi_type);
				if (dq == NODQUOT) {
					debug(LOG_DEBUG,"%5d: add ugid (%s %u)\n",
						i + j, type2name(buf[j].qi_type), buf[j].qi_id);
					dq = add_dquot_(ugid_stat, buf[j].qi_id, buf[j].qi_type);
				}
				else {
					debug(LOG_DEBUG, "%5d: update ugid (%s %u)\n",
						i + j, type2name(buf[j].qi_type), buf[j].qi_id);
				}
				memcpy( &(dq->obj.istat), &buf[j], sizeof(struct vz_quota_iface));
				dq->obj.flags &= ~UGID_LOADED;
			}
		}
		free(buf);

		/* delete entries destroyed by kernel */
		drop_ugid_by_flags(ugid_stat, UGID_LOADED);

		/* set number of ugid objects in buffer */
		ugid_stat->info.buf_size = ugid_stat->dquot_size;

		/* reset number of loaded ugid items */
		ugid_stat->info.config.count = 0;

	}
/*TODO see upper...
 *	  else if ((ugid_stat->info.config.flags & VZDQUG_ON)
			&& ugid_stat->info.config.count)
		reset_ugid_flags(ugid_stat, UGID_LOADED);
*/

	/* drop user/group quota kernel status */
	ugid_stat->info.config.flags &= ~VZDQUG_ON;

destroy:
	/* destroy quota */
	rc = vzquotactl_syscall(VZ_DQ_DESTROY, quota_id, NULL, NULL);
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota destroy syscall for id %d", quota_id);
	if (broken) {
		errno = broken;
		return -1;
	}

	return 0;
}

/* get quota stat
 * return 0 if success,
 * <0 if quota is off */
int quota_syscall_stat(struct qf_data *qd, int no_ugid_stat)
{
	int rc;
	int loop_counter;
	struct vz_quota_stat *stat;
	struct ugid_quota *ugid_stat;

	ASSERT(qd);
	stat = &qd->stat;
	ugid_stat = &qd->ugid_stat;

	/* get VE stat */
	rc = vzquotactl_syscall(VZ_DQ_GETSTAT, quota_id, stat, NULL);
	if (rc < 0) {
		if (errno == ENOENT)
			return rc;
		else
			error(EC_VZCALL, errno, "Quota getstat syscall for id %d", quota_id);
	}

	/* return if no ugid stat is required */
	if (no_ugid_stat) return 0;

	loop_counter = 0;
ugid_loop:
	if (loop_counter++ > 25)
		error(EC_VZCALL, 0, "Can't get all ugid records at once");

	/* get ugid config */
	rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_GETCONFIG, quota_id, 0, 0,
			&(ugid_stat->info.config));
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota ugid_getconfig syscall for id %d", quota_id);

	/* get ugid graces */
	if (ugid_stat->info.config.flags & VZDQUG_ON) {
		rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_GETGRACE, quota_id, 0, 0,
				&(ugid_stat->info.ugid_info));
		if (rc < 0)
			error(EC_VZCALL, errno, "Quota ugid_getgrace for id %d", quota_id);
	}

	/* get ugid objects */
	if ((ugid_stat->info.config.flags & VZDQUG_ON)
			&& ugid_stat->info.config.count
			&& ugid_stat->info.config.limit) {

		struct dquot *dq;
		unsigned int b_size, j;
		struct vz_quota_iface *buf;
		struct dquot **dq_buf = NULL;

		/* get the whole stat to minimize quota inconsistency */
		b_size = ugid_stat->info.config.count + 100;
		buf = xmalloc(b_size * sizeof(struct vz_quota_iface));

		debug(LOG_DEBUG, "get ugids from %u to %u...", 0, b_size);
		rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_GETSTAT, quota_id, 0, b_size, buf);
		if (rc < 0) {
			free(buf);
			error(EC_VZCALL, errno, "Quota ugid_getstat syscall for id %d", quota_id);
		}
		debug(LOG_DEBUG, " %u received\n", rc);

		/* loop while we have all kernel ugid at once */
		if ((unsigned)rc == b_size) {

			free(buf);
			goto ugid_loop;
		}
		if (rc > 0)
			dq_buf = xmalloc(b_size * sizeof(struct dquot *));

		for (j = 0; j < (unsigned)rc; j++) {
			if (buf[j].qi_type >= MAXQUOTAS)
				error(EC_VZCALL, 0, "Quota ugid_getstat syscall for id %d returned "
					"ugid object with invalid type (id=%u, type=%u)",
					quota_id, buf[j].qi_id, buf[j].qi_type);

			dq = lookup_dquot_(ugid_stat, buf[j].qi_id, buf[j].qi_type);
			if (dq == NODQUOT) {
				debug(LOG_DEBUG,"%5d: add ugid (%s %u)\n",
					j, type2name(buf[j].qi_type), buf[j].qi_id);
				dq = add_dquot_(ugid_stat, buf[j].qi_id, buf[j].qi_type);
			} else {
				debug(LOG_DEBUG, "%5d: update ugid (%s %u)\n",
					j, type2name(buf[j].qi_type), buf[j].qi_id);
			}
			memcpy( &(dq->obj.istat), &buf[j], sizeof(struct vz_quota_iface));
			/* handle deleted entries */
			dq->obj.flags &= ~UGID_LOADED;
			dq_buf[j] = dq;
		}
		free(buf);
		debug(LOG_DEBUG, "received %d ugids were processed\n", rc);

		/* delete entries destroyed by kernel */
		drop_ugid_by_flags(ugid_stat, UGID_LOADED);
		/* handle deleted entries */
		for (j = 0; j < (unsigned)rc; j++) {
			debug(LOG_DEBUG,"%5d: mark ugid (%s %u) as loaded\n",
				j, type2name(dq_buf[j]->obj.istat.qi_type),
				dq_buf[j]->obj.istat.qi_id);
			dq_buf[j]->obj.flags |= UGID_LOADED;
		}
		if (dq_buf) free(dq_buf);

		/* set number of ugid objects in buffer */
		ugid_stat->info.buf_size = ugid_stat->dquot_size;
	}

	return 0;
}

/* set quota limits
 * return 0 if success,
 * <0 if quota is off */
int quota_syscall_setlimit(struct qf_data *qd, int no_stat, int no_ugid_stat)
{
	int rc;
	struct vz_quota_ugid_stat config;
	struct vz_quota_stat *stat;
	struct ugid_quota *ugid_stat;

	ASSERT(qd);
	stat = &qd->stat;
	ugid_stat = &qd->ugid_stat;

	/* set VE limits */
	if (!no_stat) {
		rc = vzquotactl_syscall(VZ_DQ_SETLIMIT, quota_id, stat, NULL);
		if (rc < 0) {
			if (errno == ENOENT)
				return rc;
			else
				error(EC_VZCALL, errno, "Quota setlimit syscall for id %d", quota_id);
		}
	}

	if (no_ugid_stat) return 0;

	/* get ugid config */
	rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_GETCONFIG, quota_id, 0, 0, &config);
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota ugid_getconfig syscall for id %d", quota_id);

	/* nothing to change */
	if (ugid_stat->info.config.limit == config.limit)
		return 0;

	/* user/group quota is inactive in kernel */
	if (!(ugid_stat->info.config.flags & VZDQUG_ON)) {
		/* debug(LOG_WARNING, "User/group quota is inactive for id %d, "
				"ugid limit change will take effect after "
				"user/group quota activation\n", quota_id);
		*/
		return 0;
	}

	/* limit was exceeded or no ugids were loaded */
	if (ugid_stat->info.config.flags & VZDQUG_FIXED_SET ||
			!config.limit) {
		debug(LOG_WARNING, "Number of ugid objects exceeded the ugid limit; "
				"ugid limit change for id %d will take "
				"effect after quota restart only\n", quota_id);
		}

	/* set ugid limit */
	rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_SETCONFIG, quota_id, 0, 0, &(ugid_stat->info.config));
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota ugid_setmax syscall for id %d", quota_id);

	/* set indicator of limit was changed: 0 -> x */
	if (!config.limit) {
		errno = EBUSY;
		return -1;
	}

	return 0;
}

static void dqstat2dqblk(struct dq_stat * ulimit,
		    struct _if_dqblk * dqb) {
	dqb->dqb_bsoftlimit = ulimit->bsoftlimit;
	dqb->dqb_bhardlimit = ulimit->bhardlimit;
	dqb->dqb_isoftlimit = ulimit->isoftlimit;
	dqb->dqb_ihardlimit = ulimit->ihardlimit;
}

static void vzdqinfo2dqinfo(struct dq_info * info,
			    struct _if_dqinfo * dqi) {
	dqi->dqi_bgrace = info->bexpire;
	dqi->dqi_igrace = info->iexpire;
}

#define VZFS_DEV		"/dev/vzfs"
static int quotactl_syscall(int cmd, int type, const char * root_path, int id, void * data)
{
	char path[PATH_MAX];
	int err, qerr = 0;
	int fd;

	/* we should chroot to VE priv dir */
	sprintf(path, "%s/root", root_path);
	fd = open("/", O_RDONLY);
	if (fd < 0)
		return -1;
	if ((err = chroot(path)) != 0)
		goto out;
	qerr = quotactl(QCMD(cmd, type), VZFS_DEV, id, data);
	if ((err = fchdir(fd)) != 0)
		goto out;
	err = chroot(".");
out:
	close(fd);

	if (qerr != 0)
		return qerr;
	return err;
}

int vzquotactl_ugid_setgrace(struct qf_data *data, int type, struct dq_info *vzdqinfo)
{
	struct vz_quota_ugid_setinfo inf;
	long err;
	char *path;

	path = get_quota_path(data);

	memset(&inf, 0, sizeof(inf));
	vzdqinfo2dqinfo(vzdqinfo, &inf.dqi);
	inf.dqi.dqi_valid = IIF_BGRACE | IIF_IGRACE;
	inf.type = type;

	err = vzquotactl_ugid_syscall(VZ_DQ_UGID_SETINFO, quota_id, 0, 0, &inf);
	if (err == -EINVAL || err == -EPERM) {
		struct mem_dqinfo _info;
		memset(&_info, 0, sizeof(_info));
		_info.dqi_bgrace = vzdqinfo->bexpire;
		_info.dqi_igrace = vzdqinfo->iexpire;

		err = quotactl_syscall(Q_SETGRACE,
				type, path, 0, (void *) &_info);
	}
	return err;
}

int vzquotactl_ugid_setlimit(struct qf_data *data, int id, int type, struct dq_stat *vzdqlim)
{
	struct vz_quota_ugid_setlimit lim;
	long err;
	char *path;

	path = get_quota_path(data);

	memset(&lim, 0, sizeof(lim));
	dqstat2dqblk(vzdqlim, &lim.dqb);
	lim.dqb.dqb_valid = QIF_LIMITS;
	lim.type = type;
	lim.id = id;

	err = vzquotactl_ugid_syscall(VZ_DQ_UGID_SETLIMIT, quota_id, 0, 0, &lim);
	if (err == -EINVAL || err == -EPERM) {
		struct mem_dqblk dqb;
		memset(&dqb, 0, sizeof(dqb));

		dqb.dqb_bsoftlimit = vzdqlim->bsoftlimit;
		dqb.dqb_bhardlimit = vzdqlim->bhardlimit;
		dqb.dqb_isoftlimit = vzdqlim->isoftlimit;
		dqb.dqb_ihardlimit = vzdqlim->ihardlimit;

		err = quotactl_syscall(Q_SETQLIM,
			type, path, id, (void *) &dqb);
	}
	return err;
}

/*
 * Return path to quota config according to VZ3 scheme
 */
static char* get_quota_file_name_vz3(unsigned int quota_id, char *buf,
								int bufsize)
{
	memset(buf, 0, bufsize);
	bufsize--;

	if (snprintf(buf, bufsize, "%s/%s.%u", VZQUOTA_FILES_PATH_VZ3,
			VZQUOTA_FILE_NAME, quota_id) >= bufsize)
		error(EC_NOQUOTAFILE, 0, "quota.%d file name is too long\n",
			quota_id);
	return buf;
}

/*
 * Return path to quota config according to VZ4 scheme (or)
 * exit on critical errors (or)
 * return NULL if quota file name can't be obtained
 */
static char *get_quota_file_name_vz4(unsigned int quota_id, char *buf,
	int bufsize)
{
	char *ve_priv;

	memset(buf, 0, bufsize);
	bufsize--;

	if (option & FL_PATH) {
		char *fs;
		int ret;
		char *p1, *p2;

		ASSERT(mount_point);

		p1 = xstrdup(mount_point);
		p2 = xstrdup(mount_point);

		ve_priv = dirname(p1);
		fs = basename(p2);

		/* ve_priv and fs can't be NULL even in case of error */

		ret = snprintf(buf, bufsize, "%s/quota.%s", ve_priv, fs);

		free(p1);
		free(p2);

		if (ret >= bufsize) {
			error(EC_NOQUOTAFILE, 0, "path to quota file is "
				"too long\n");
			return NULL;
		}
	} else {
		/*
		 * mount point is not set.
		 *
		 * TODO:
		 * We have to parse VE config file to obtain path to VE
		 * private and use default relative path to 'fs'.
		 */
		return NULL;
	}

	return buf;
}

/*
 * Return path to quota config file for given quota_id, or exit with error
 */
char *get_quota_file_name(unsigned int quota_id, char *buf, int bufsize,
							int flags)
{
	char *name;
	struct stat s;
	int ret;

	if (flags & O_CREAT) {
		/*
		 * vzquota init QID -p path [-R]
		 *
		 * We just want to create a quota file. The -p /PATH/TO/FS
		 * option (path to quota accounting point) must be used with
		 * vzquota init.
		 *
		 * New files can be created in two places, it depends on
		 * -R option, that specifies do we want to use relative path
		 * or absolute path to accounting point:
		 *
		 *  (1) The -R option is set:
		 *	quota config file is /PATH/TO/quota.FS, i.e.
		 *	in most cases /vz/private/ID/quota.fs
		 *
		 *  (2) The -R option is omitted:
		 *	quota config file will be created in old (VZ3) place:
		 *	/var/vzquota/quota.QID
		 */
		if (option & FL_RELATIVE)
			name = get_quota_file_name_vz4(quota_id, buf, bufsize);
		else
			name = get_quota_file_name_vz3(quota_id, buf, bufsize);
		debug(LOG_DEBUG, "Create config file %s\n", name);
		return name;
	}

	/*
	 * vzquota on/off/drop/... QID
	 *
	 * Try to find quota file according to VZ4 format first.
	 */
	name = get_quota_file_name_vz4(quota_id, buf, bufsize);

	/*
	 * Now we want just to ensure, that VZ4 quota config file exists, and
	 * if not - fall back to VZ3 path. Actual open() will be done later in
	 * open_quota_file().
	 */
	if (name) {
		ret = stat(name, &s);
		if (ret < 0)
			ret = errno;
	} else
		ret = ENOENT;

	switch (ret) {
	case 0:
		/* Ok, VZ4 file looks alive */
		debug(LOG_DEBUG, "Open: looking for %s ... ok\n", name);
		break;
	case ENOENT:
		if (name)
			debug(LOG_DEBUG, "Open: looking for %s ... failed\n",
									name);
		/*
		 * Backward compatibility, try to find it in old (VZ3) place
		 * This behaviour is disabled when the -R option is used.
		 */
		if (!(option & FL_RELATIVE)) {
			name = get_quota_file_name_vz3(quota_id, buf, bufsize);
			debug(LOG_DEBUG, "set quota config name to %s\n", name);
			break;
		}
		/* Fall through ... */
	default:
		error(EC_NOQUOTAFILE, errno,
			"can't open quota config file %s\n", name);
	}

	return name;
}

int get_quota_version(struct vz_quota_header *head)
{
	switch(head->magic) {
		case MAGIC_V3:	return QUOTA_V3;
		case MAGIC_V2:	return QUOTA_V2;
		case MAGIC_V1:	return QUOTA_V1;
	}
	error(0, 0, "Can't detect quota file version; file is not a VZ quota file");
	return -1;
}

int unlink_quota_file(unsigned int quota_id, const char *name)
{
	char buf[MAXFILENAMELEN];
	int status;

	if (!name)
		name = get_quota_file_name(quota_id, buf, MAXFILENAMELEN, 0);

	if ((status = unlink(name)) < 0)
	{
		error(0, errno, "unlink quota file '%s'", name);
		return status;
	}

	return 0;
}

static int reformat_quota (int fd)
{
	int rc;
	struct qf_data qd;

	init_quota_data(&qd);
	rc = read_quota_file(fd, &qd, IOF_ALL);
	if (rc < 0) return rc;

	/* v2 -> v3 requires quota recalculation */
	if (qd.version == QUOTA_V2)
		qd.head.flags |= QUOTA_DIRTY;

	/* v1 -> v3 */
	//TODO saw@ thinks about recalculation

	rc = write_quota_file(fd, &qd, IOF_ALL);
	if (rc < 0) return rc;
	return 0;
}

/* computes checksum of quota file, checksum being in the end of file */
int chksum_quota_file(int fd, chksum_t *chksum)
{
	off_t fsize;
	size_t b_size, size, i, j;
	chksum_t *buf;
	int err;

	fsize = lseek(fd, 0, SEEK_END);
	if (fsize <= 0) return EC_QUOTAFILE;
	fsize -= sizeof(chksum_t);
	debug(LOG_DEBUG, "Computing hash of quota file (first %u bytes)\n", fsize);

	memset(chksum, 0, sizeof(chksum_t));

	b_size = (IO_BUF_SIZE / sizeof(chksum_t)) * sizeof(chksum_t);
	buf = xmalloc(b_size);

	for (i = 0; i < fsize; i += b_size) {
		size = (i + b_size < fsize) ? b_size : fsize - i;
		memset(buf, 0, b_size);
		err = read_field(fd, buf, size, i);
		if (err < 0) {
			free(buf);
			return err;
		}
		for (j = 0; j < b_size / sizeof(chksum_t); j++)
			*chksum ^= buf[j];
	}
	debug(LOG_DEBUG, "Hash is 64bit number %llu\n", *chksum);

	free(buf);
	return 0;
}

int do_check_quota_file(int fd, int reformat)
{
	int rc;
	struct qf_data qd;

	init_quota_data(&qd);
	rc = read_quota_file(fd, &qd, IOF_HEAD);
	if (rc < 0) return rc;

	if (qd.version != QUOTA_CURRENT)
	{
		if (!reformat) {
			debug(LOG_WARNING, "Quota file for id %d is in old quota format");
			return -1;
		}
		/* convert quota */
		if (reformat_quota(fd) < 0) {
			free_quota_data(&qd);
			return -1;
		}
	} else {
		/* verify checksum*/
		chksum_t chksum;
		rc = read_quota_file(fd, &qd, IOF_CHKSUM);
		if (rc < 0) return rc;
		chksum_quota_file(fd, &chksum);
		if (memcmp(&chksum, &qd.chksum, sizeof(chksum_t))) {
			debug(LOG_ERROR, "Quota file check sum is invalid for id %d; "
				"file is broken\n", quota_id);
			return -1;
		}
	}
	free_quota_data(&qd);
	return 0;
}


int check_quota_file(int fd)
{
	return do_check_quota_file(fd, 1);
}


int open_quota_file(unsigned int quota_id, const char *name, int flags)
{
	int fd;
	char buf[MAXFILENAMELEN];

	if (!name)
		name = get_quota_file_name(quota_id, buf, MAXFILENAMELEN,
									flags);

	actual_config_file = xstrdup(name);

	if ((fd = open(name, flags, 00600)) < 0)
	{
		if (!(flags & O_CREAT)) {
			error(0, errno, "Can't open quota file for id %d, "
			      "maybe you need to reinitialize quota", quota_id);
		} else if (errno != EEXIST)
			error(0, errno, "create quota file '%s'", name);
		return fd;
	}

	if (flags & O_RDWR) {
		if (flock(fd, LOCK_EX | LOCK_NB) < 0)
			error(EC_LOCK, 0, "can't lock quota file, some quota operations "
					"are performing for id %d", quota_id);
	}
	debug(LOG_DEBUG, "file %s %d was opened\n", name, fd);
	return fd;
}

int close_quota_file(int fd)
{
	fsync(fd);
	flock(fd, LOCK_UN);
	debug(LOG_DEBUG, "file %d was closed\n", fd);
	return close(fd);
}

int read_field(int fd, void *field, size_t size, off_t offset)
{
	int err = pread(fd, field, size, offset);
	if (err < 0) {
		error(0, errno, "read quota file");
		return err;

	} else if ((unsigned)err != size) {
		error(0, 0, "quota file is corrupted");
		return -E_FILECORRUPT;
	}

	return 0;
}

int read_quota_file(int fd, struct qf_data *q, int io_flags)
{
	off_t err;
	int struct_size = sizeof(struct vz_quota_stat);
	debug(LOG_DEBUG, "Start reading quota file\n");

	/* read header */
	if (io_flags & IOF_HEAD) {
		debug(LOG_DEBUG, "Reading header from file\n");
		err = read_field(fd, &(q->head), sizeof(struct vz_quota_header), QF_OFF_HEADER);
		if (err < 0) return err;

		/* get version */
		q->version = get_quota_version( &(q->head));
		if (q->version < 0)
			return -1;
	}

	/* read 1-level quota stat */
	if (io_flags & IOF_STAT) {
		void *st;

		debug(LOG_DEBUG, "Reading 1-level quota stat from file\n");
		switch(q->version) {
			case QUOTA_V1:	struct_size = sizeof(struct vz_quota_stat_old1);
					break;
			case QUOTA_V2:	struct_size = sizeof(struct vz_quota_stat_old2);
					break;
			case QUOTA_V3:	struct_size = sizeof(struct vz_quota_stat);
					break;
			default :	error(0, 0, "File is not a VZ quota file");
					return -1;
		}
		st = xmalloc(struct_size);

		if ((err = read_field(fd, st, struct_size, QF_OFF_STAT)) < 0)
			return err;

		convert_quota_stat( &(q->stat), QUOTA_CURRENT, st, q->version);
		free(st);
	}

	/* read path */
	if (io_flags & IOF_PATH)
	{
		debug(LOG_DEBUG, "Reading mount point path from file\n");
		err = read_field(fd, &(q->path_len), sizeof(size_t), QF_OFF_PATH_LEN(struct_size));
		if (err < 0) return err;

		if (q->path) free(q->path);
		q->path = NULL;

		if (q->path_len) {
			q->path = xmalloc( q->path_len + 1);
			err = read_field(fd, q->path, q->path_len,
				QF_OFF_PATH(struct_size));
			if (err < 0) return err;
		}
		debug(LOG_DEBUG, "Read quota path: %s\n",
			q->path ? q->path : "[relative]");
	}

	/* read 2-level quota info */
	if (io_flags & IOF_UGID_INFO) {
		clean_ugid_info(&(q->ugid_stat));
		if (q->version == QUOTA_CURRENT) {
			debug(LOG_DEBUG, "Reading 2-level quota info from file\n");

			err = read_field(fd, &(q->ugid_stat.info), sizeof(struct ugid_info),
				QF_OFF_UGID_INFO(q->path_len));
			if (err < 0) return err;
		}
	}

	/* read ugid objects */
	if (io_flags & IOF_UGID_BUF) {
		free_ugid_quota(&(q->ugid_stat));
		if (q->version == QUOTA_CURRENT) {

			debug(LOG_DEBUG, "Reading ugid objects from file: %u entries total\n",
				q->ugid_stat.info.buf_size);

			if (q->ugid_stat.info.buf_size > 0) {

				size_t b_size, size, i, j;
				struct ugid_obj *buf;
				struct dquot *dq;

				b_size = IO_BUF_SIZE / sizeof(struct ugid_obj);
				buf = xmalloc(b_size * sizeof(struct ugid_obj));

				for (i = 0; i < q->ugid_stat.info.buf_size; i += b_size) {
					size = (i + b_size < q->ugid_stat.info.buf_size) ? b_size : q->ugid_stat.info.buf_size - i;
					err = read_field(fd, buf, size * sizeof(struct ugid_obj),
						QF_OFF_UGID_BUF(q->path_len) + i * sizeof(struct ugid_obj));
					if (err < 0) {
						free(buf);
						return err;
					}
					for (j = 0; j < size; j++) {
						if (buf[j].istat.qi_type >= MAXQUOTAS)
							error(EC_QUOTAFILE, 0, "%u ugid object has incorrect type: id=%u, type=%u; "
								"quota file is corrupted",
								i*b_size + j, buf[j].istat.qi_id, buf[j].istat.qi_type);
						dq = lookup_dquot(&(q->ugid_stat), &buf[j]);
						if (dq == NODQUOT)
							dq = add_dquot(&(q->ugid_stat), &buf[j]);
						else
							memcpy( &(dq->obj), &buf[j], sizeof(struct ugid_obj));
					}
				}
				free(buf);
			}
		}
		if (q->ugid_stat.dquot_size != q->ugid_stat.info.buf_size)
			error(EC_QUOTAFILE, 0, "Number of ugid objects read from quota file is not "
				"equal to one in 2-level quota statistics; "
				"quota file is corrupted for id %d\n", quota_id);
	}

	/* read checksum */
	if ((io_flags & IOF_CHKSUM) && (q->version >= QUOTA_V3)) {
		off_t fsize;
		debug(LOG_DEBUG, "Reading quota file checksum\n");

		fsize = lseek(fd, 0, SEEK_END);
		if (fsize <= 0) return EC_QUOTAFILE;
		err = read_field(fd, &q->chksum, sizeof(q->chksum), fsize - sizeof(q->chksum));
		if (err < 0) return err;
		debug(LOG_DEBUG, "Checksum is 64bit number %llu\n", q->chksum);
	}

	debug(LOG_DEBUG, "Quota file was read\n");
	return 0;
}

int write_field(int fd, const void *field, size_t size, off_t offset)
{
	int err = pwrite(fd, field, size, offset);
	if (err < 0 || (unsigned)err != size)
	{
		error(0, errno, "write quota file");
		return err;
	}
	return 0;
}

/* this function should be called with io_flags=IOF_ALL cause of checksum */
int write_quota_file(int fd, struct qf_data *q, int io_flags)
{
	off_t err;

	q->version = QUOTA_CURRENT;
	q->head.magic = MAGIC_CURRENT;

	debug(LOG_DEBUG, "Start writing quota file\n");

	/* write header */
	if (io_flags & IOF_HEAD) {
		debug(LOG_DEBUG, "Writing header to file\n");
		err = write_field(fd, &(q->head), sizeof(struct vz_quota_header), QF_OFF_HEADER);
		if (err < 0) return err;
	}

	/* write 1-level quota stat */
	if (io_flags & IOF_STAT) {
		debug(LOG_DEBUG, "Writing 1-level quota stat to file\n");
		err = write_field(fd, &(q->stat), sizeof(struct vz_quota_stat), QF_OFF_STAT);
		if (err < 0) return err;
	}

	/* write path */
	if (io_flags & IOF_PATH)
	{
		debug(LOG_DEBUG, "Writing mount point path to file\n");
		q->path_len = q->path ? strlen(q->path) : 0;

		err = write_field(fd, &(q->path_len), sizeof(size_t),
				  QF_OFF_PATH_LEN(sizeof(struct vz_quota_stat)));
		if (err < 0) return err;

		err = write_field(fd, q->path, q->path_len,
				  QF_OFF_PATH(sizeof(struct vz_quota_stat)));
		if (err < 0) return err;
	}

	/* write 2-level quota info */
	if (io_flags & IOF_UGID_INFO) {
		debug(LOG_DEBUG, "Writing 2-level quota info to file\n");

		 /* at this point info.buf_size MUST be equal to dquot_size */
		err = write_field(fd, &(q->ugid_stat.info), sizeof(struct ugid_info),
			QF_OFF_UGID_INFO(q->path_len));
		if (err < 0) return err;
	}

	/* write ugid objects */
	if (io_flags & IOF_UGID_BUF) {

		debug(LOG_DEBUG, "Writing ugid objects to file: %u entries total\n",
			q->ugid_stat.dquot_size);

		if (q->ugid_stat.info.buf_size != q->ugid_stat.dquot_size)
			error(EC_QUOTAFILE, 0, "Number of stored ugids objects is not equal to "
				"one in 2-level quota statistics; "
				"quota file is corrupted for id %d\n", quota_id);

		if (q->ugid_stat.dquot_size > 0) {

			size_t b_size, size, i, j;
			struct ugid_obj *buf;
			struct dquot *dq;

			b_size = IO_BUF_SIZE / sizeof(struct ugid_obj);
			buf = xmalloc(b_size * sizeof(struct ugid_obj));

			reset_dquot_search();
			size = 0;
			for (i = 0; ; i++) {
				dq = get_next_dquot(&(q->ugid_stat));
				if ((dq == NODQUOT) || (i >= b_size)) {
					j = (i >= b_size) ? b_size : i;
					err = write_field(fd, buf, j * sizeof(struct ugid_obj),
						QF_OFF_UGID_BUF(q->path_len) + size * sizeof(struct ugid_obj));
					if (err < 0) {
						free(buf);
						return err;
					}
					size += j;
					i = 0;
				}
				if (dq == NODQUOT) break;
				memcpy( &buf[i], &(dq->obj), sizeof(struct ugid_obj));
			}
			free(buf);

			q->ugid_stat.info.buf_size = size;
			if (q->ugid_stat.info.buf_size != q->ugid_stat.dquot_size)
				error(EC_QUOTAFILE, 0, "Number of saved ugids objects is not equal to "
					"one in 2-level quota statistics; "
					"quota file is corrupted for id %d\n", quota_id);
		}

		/* truncate file */
		err = ftruncate(fd, QF_OFF_UGID_BUF(q->path_len) +
			q->ugid_stat.dquot_size * sizeof(struct ugid_obj));
		if (err < 0) return err;
	}

	/* compute and write checksum */
	if (io_flags & IOF_CHKSUM) {
		off_t fsize;
		debug(LOG_DEBUG, "Writing quota file checksum\n");

		/* complete file with null checksum */
		fsize = lseek(fd, 0, SEEK_END);
		if (fsize <= 0) return EC_QUOTAFILE;
		memset(&q->chksum, 0, sizeof(q->chksum));
		err = write_field(fd, &q->chksum, sizeof(q->chksum), fsize);
		if (err < 0) return err;

		/* compute checksum */
		err = chksum_quota_file(fd, &q->chksum);
		if (err < 0) return err;

		/* write checksum */
		err = write_field(fd, &q->chksum, sizeof(q->chksum), fsize);
		if (err < 0) return err;
	}

	debug(LOG_DEBUG, "Quota file was written\n");
	return 0;
}
