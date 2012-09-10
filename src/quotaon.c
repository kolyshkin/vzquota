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

#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "vzquota.h"
#include "common.h"
#include "quota_io.h"
#include "quotacheck.h"

/* quota init */

static char quotainit_usage[] =
"Usage: %s %s <quotaid> -p <mount_path> [-c <quota_file>]\n"
"\t[-R,--relative]\n"
"\t[-s,--sub-quotas 1|0]\n"
"\t[-u,--ugid-limit <ugid_limit>]\n"
"\t-b,--block-softlimit <block_soft_limit>\n"
"\t-B,--block-hardlimit <block_hard_limit>\n"
"\t-i,--inode-softlimit <inode_soft_limit>\n"
"\t-I,--inode-hardlimit <inode_hard_limit>\n"
"\t-e,--block-exptime <block_expiration_time>\n"
"\t-n,--inode-exptime <inode_expiration_time>\n";

static char quotainit_short_options[] = "p:c:r:s:u:" "b:B:i:I:e:n:R";
static struct option quotainit_long_options[] = {
	{"rsquash", required_argument, NULL, 'r'}, /* depricated */
	{"sub-quotas", required_argument, NULL, 's'},
	{"ugid-limit", required_argument, NULL, 'u'},
	{"quota-file", required_argument, NULL, 'c'},

	{"block-softlimit", required_argument, NULL, 'b'},
	{"block-hardlimit", required_argument, NULL, 'B'},
	{"block-exptime", required_argument, NULL, 'e'},

	{"inode-softlimit", required_argument, NULL, 'i'},
	{"inode-hardlimit", required_argument, NULL, 'I'},
	{"inode-exptime", required_argument, NULL, 'n'},
	{"relative", no_argument, NULL, 'R'},
	{0, 0, 0, 0}
};

/* quota on */

static char quotaon_usage[] =
"Usage: %s %s <quotaid> [-p <mount_path>] [-c <quota_file>]\n"
"\t[-R,--relative]\n"
"\t[-f,--force]\n"
"\t[-s,--sub-quotas 1|0]\n"
"\t[-u,--ugid-limit <ugid_limit>]\n"
"\t[-b <block_soft_limit>] [-B <block_hard_limit>]\n"
"\t[-i <inode_soft_limit>] [-I <inode_hard_limit>]\n"
"\t[-e <block_expiration_time>] [-n <inode_expiration_time>]\n";

static char quotaon_short_options[] = "p:c:r:s:u:f" "b:B:i:I:e:n:R";
static struct option quotaon_long_options[] = {
	{"rsquash", required_argument, NULL, 'r'}, /* depricated */
	{"sub-quotas", required_argument, NULL, 's'},
	{"ugid-limit", required_argument, NULL, 'u'},
	{"quota-file", required_argument, NULL, 'c'},
	{"nocheck", no_argument, NULL, 13},

	{"block-softlimit", required_argument, NULL, 'b'},
	{"block-hardlimit", required_argument, NULL, 'B'},
	{"block-exptime", required_argument, NULL, 'e'},

	{"inode-softlimit", required_argument, NULL, 'i'},
	{"inode-hardlimit", required_argument, NULL, 'I'},
	{"inode-exptime", required_argument, NULL, 'n'},
	{"relative", no_argument, NULL, 'R'},
	{"force", no_argument, NULL, 'f'},
	{0, 0, 0, 0}
};

/* quota setlimit */

static char quotaset_usage[] =
"Usage: %s %s <quotaid> [-p <mount_path>] [-c <quota_file>]\n"
"\t[-R,--relative]\n"
"\t[-f,--force]\n"
"\t[-s,--sub-quotas 1|0]\n"
"\t[-u,--ugid-limit <ugid_limit>]\n"
"\t[-b <block_soft_limit>] [-B <block_hard_limit>]\n"
"\t[-i <inode_soft_limit>] [-I <inode_hard_limit>]\n"
"\t[-e <block_expiration_time>] [-n <inode_expiration_time>]\n";

static char quotaset_short_options[] = "p:c:r:s:u:f" "b:B:i:I:e:n:R";
static struct option quotaset_long_options[] = {
	{"rsquash", required_argument, NULL, 'r'}, /* depricated */
	{"sub-quotas", required_argument, NULL, 's'},
	{"ugid-limit", required_argument, NULL, 'u'},
	{"quota-file", required_argument, NULL, 'c'},

	{"block-softlimit", required_argument, NULL, 'b'},
	{"block-hardlimit", required_argument, NULL, 'B'},
	{"block-exptime", required_argument, NULL, 'e'},

	{"inode-softlimit", required_argument, NULL, 'i'},
	{"inode-hardlimit", required_argument, NULL, 'I'},
	{"inode-exptime", required_argument, NULL, 'n'},
	{"relative", no_argument, NULL, 'R'},
	{"force", no_argument, NULL, 'f'},
	{0, 0, 0, 0}
};

/* quota ugidset */

static char quotaugidset_usage[] =
"Usage: \n"
"\t%s %s <quotaid> [-p <mount_path>] [-c <quota_file>]\n"
"\t\t[-R,--relative]\n"
"\t\t[-u|-g] <ugid>\n"
"\t\t<block_soft_limit> <block_hard_limit>\n"
"\t\t<inode_soft_limit> <inode_hard_limit>\n"
"\t%1$s %2$s <quotaid> [-p <mount_path>] [-c <quota_file>] -t\n"
"\t\t[-R,--relative]\n"
"\t\t<block-grace> <inode-grace>\n";

static char quotaugidset_short_options[] = "p:c:ugtR";

static struct option quotaugidset_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{"uid", required_argument, NULL, 'u'},
	{"gid", required_argument, NULL, 'g'},
	{"relative", no_argument, NULL, 'R'},
	{0, 0, 0, 0}
};

/* quota off */

static char quotaoff_usage[] =
"Usage: %s %s <quotaid> [-p <mount_path>] [-c <quota_file>]\n"
"\t[-R,--relative]\n"
"\t[-f,--force]\n";

static char quotaoff_short_options[] = "p:fc:R";
static struct option quotaoff_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{"relative", no_argument, NULL, 'R'},
	{"force", no_argument, NULL, 'f'},
	{0, 0, 0, 0}
};

/* quota drop */

static char quotadrop_usage[] =
"Usage: %s %s <quotaid> [-p <mount_path>] [-c <quota_file>]\n"
"\t[-R,--relative]\n"
"\t[-f,--force]\n";

static char quotadrop_short_options[] = "p:fc:R";
static struct option quotadrop_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{"relative", no_argument, NULL, 'R'},
	{"force", no_argument, NULL, 'f'},
	{0, 0, 0, 0}
};

/* quota reload */

static char quotareloadugid_usage[] =
"Usage: %s %s <quotaid> [-p <mount_path>] [-c <quota_file>]\n"
"\t[-R,--relative]\n";

static char quotareloadugid_short_options[] = "p:c:R";
static struct option quotareloadugid_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{"relative", no_argument, NULL, 'R'},
	{0, 0, 0, 0}
};

static void set_quotas_from_line(struct qf_data *qd)
{
	struct vz_quota_stat *stat;
	struct ugid_quota *ugid_stat;

	ASSERT(qd);
	stat = &qd->stat;
	ugid_stat = &qd->ugid_stat;

	/* user enters 1k blocks */
	if (option & FL_BHL)
		stat->dq_stat.bhardlimit = block2ker(limits.dq_stat.bhardlimit);

	if (option & FL_BSL)
		stat->dq_stat.bsoftlimit = block2ker(limits.dq_stat.bsoftlimit);

	if (option & FL_BET)
		stat->dq_info.bexpire = limits.dq_info.bexpire;

	if (option & FL_IHL)
		stat->dq_stat.ihardlimit = limits.dq_stat.ihardlimit;
	if (option & FL_ISL)
		stat->dq_stat.isoftlimit = limits.dq_stat.isoftlimit;
	if (option & FL_IET)
		stat->dq_info.iexpire = limits.dq_info.iexpire;

	if (option & FL_SQT) {
		/* mark quota dirty if ugid quota is changing OFF -> ON */
		if (!(qd->head.flags & QUOTA_UGID_ON) &&
		//if (!(ugid_stat->info.config.flags & VZDQUG_ON) &&
		    (ug_config.flags & VZDQUG_ON))
			qd->head.flags |= QUOTA_DIRTY;

		qd->head.flags &= ~QUOTA_UGID_ON;
		if (ug_config.flags & VZDQUG_ON)
			qd->head.flags |= QUOTA_UGID_ON;
	}
	if (option & FL_UGL) {
		/* mark quota dirty if ugid quota is ON and limit is changing as 0 -> X */
		if ( (!ugid_stat->info.config.limit) &&
				ug_config.limit &&
				//(ug_config.flags & VZDQUG_ON))
				(qd->head.flags & QUOTA_UGID_ON))
			qd->head.flags |= QUOTA_DIRTY;

		ugid_stat->info.config.limit = ug_config.limit;
	}
}

static void check_limits(struct qf_data *qd, int what_given)
{
	struct vz_quota_stat *qstat;
	struct ugid_quota *ugid_stat;
	struct dq_stat *stat;

	ASSERT(qd);
	qstat = &(qd->stat);
	ugid_stat = &(qd->ugid_stat);
	stat = &(qstat->dq_stat);

	if ((stat->bhardlimit < stat->bsoftlimit)
				&& (what_given ? (option & (FL_BHL | FL_BSL)) : 1))
		error(EC_USAGE, 0, "block_hard_limit [%u] < block_soft_limit [%u]\n",
		      ker2block(stat->bhardlimit), ker2block(stat->bsoftlimit));

	if ((stat->ihardlimit < stat->isoftlimit)
				&& (what_given ? (option & (FL_IHL | FL_ISL)) : 1))
		error(EC_USAGE, 0, "inode_hard_limit [%u] < inode_soft_limit [%u]\n",
		      stat->ihardlimit, stat->isoftlimit);

	if ((stat->bhardlimit < stat->bcurrent)
				&& (what_given ? (option & FL_BHL) : 1))
		debug(LOG_WARNING, "block_hard_limit [%u] < block_current_usage [%u]\n",
		      ker2block(stat->bhardlimit), ker2block(stat->bcurrent));
	else if ((stat->bsoftlimit < stat->bcurrent)
				&& (what_given ? (option & FL_BSL) : 1))
		debug(LOG_WARNING, "block_soft_limit [%u] < block_current_usage [%u]\n",
			ker2block(stat->bsoftlimit), ker2block(stat->bcurrent));

	if ((stat->ihardlimit < stat->icurrent)
				&& (what_given ? (option & FL_IHL) : 1))
		debug(LOG_WARNING, "inode_hard_limit [%u] < inode_current_usage [%u]\n",
		      stat->ihardlimit, stat->icurrent);
	else if ((stat->isoftlimit < stat->icurrent)
				&& (what_given ? (option & FL_ISL) : 1))
		debug(LOG_WARNING, "inode_soft_limit [%u] < inode_current_usage [%u]\n",
		      stat->isoftlimit, stat->icurrent);

	if (!(qd->head.flags & QUOTA_UGID_ON)) return;

	if ((ugid_stat->info.config.limit < ugid_stat->info.buf_size)
				&& (what_given ? (option & FL_UGL) : 1))
		debug(LOG_WARNING, "ugid_limit [%u] < ugid_current_usage [%u]\n",
		      ugid_stat->info.config.limit, ugid_stat->info.buf_size);

	//TODO check limits for ugids -> warnings
}

static void correct_grace(struct vz_quota_stat *stat, struct ugid_quota *ugid_stat)
{
	struct dquot *dq;
	struct dq_stat *s;
	struct dq_info *i;
	int type;
	time_t nowt;

	ASSERT(stat);

	nowt = time(NULL);

	/* VE quota */
	if (stat) {
		s = &(stat->dq_stat);
		i = &(stat->dq_info);

		if (s->bcurrent <= s->bsoftlimit)
			s->btime = 0;
		else if (s->btime == 0)
			s->btime = nowt + i->bexpire;

		if (s->icurrent <= s->isoftlimit)
			s->itime = 0;
		else if (s->itime == 0)
			s->itime = nowt + i->iexpire;
	}

	/* ugid quota */
	if (ugid_stat) {
		reset_dquot_search();
		while ((dq = get_next_dquot(ugid_stat)) != NODQUOT) {

			s = &(dq->obj.istat.qi_stat);
			type = dq->obj.istat.qi_type;
			i = &(ugid_stat->info.ugid_info[type]);

			if ((s->bcurrent <= s->bsoftlimit) || !s->bsoftlimit)
				s->btime = 0;
			else if (s->btime == 0)
				s->btime = nowt + i->bexpire;

			if ((s->icurrent <= s->isoftlimit) || !s->isoftlimit)
				s->itime = 0;
			else if (s->itime == 0)
				s->itime = nowt + i->iexpire;
		}
	}
}

static void calc_current_usage(struct qf_data *qd)
{
	struct scan_info info;
	char *path;

	path = get_quota_path(qd);

	/* whether to scan user/group info */
	if (qd->head.flags & QUOTA_UGID_ON)
		info.ugid_stat = &(qd->ugid_stat);
	else
		info.ugid_stat = NULL;

	scan(&info, path);
	qd->stat.dq_stat.bcurrent = info.size;
	qd->stat.dq_stat.icurrent = info.inodes;

	qd->head.flags &= ~QUOTA_DIRTY;
}

struct quota_file_t {
	unsigned int id;
	const char * path;
};

void quota_file_cleaner(void * data) {
	struct quota_file_t * qf = (struct quota_file_t *) data;
	unlink_quota_file(qf->id, qf->path);
}

static void quota_init()
{
	int fd;
	int rc;
	struct qf_data qd;

	init_quota_data(&qd);

	ASSERT(mount_point);

	rc = quota_syscall_stat(&qd, 1);
	if (rc == 0)
		error(EC_RUN, 0, "Quota is running, stop it first");

	fd = open_quota_file(quota_id, config_file, O_RDWR|O_CREAT|O_EXCL);
	if (fd < 0) {
		if (errno == EEXIST)
		{
			debug(LOG_WARNING, "Quota file exists, it will be overwritten\n");
			/*
			 * Don't remove file here, just try to reopen it without
			 * O_EXCL. If we remove it now, then in case of error
			 * later we'll:
			 * - lost all data for this quota.id;
			 * - get empty (== corrupted) quota file.
			 */
			fd = open_quota_file(quota_id, config_file,
								O_RDWR|O_CREAT);
		}
		if (fd < 0)
			exit(EC_QUOTAFILE);
	} else {
		/* add remove created quota file in case of error */
		struct quota_file_t * qf = xmalloc(sizeof(struct quota_file_t));
		qf->id = quota_id;
		/*
		 * Use actual_config_file, because config_file can be NULL,
		 * but we've just created a file in the default place.
		 *
		 * NB! Internal redesign is required - the config_file
		 * pointer is global, but it's also used as an argument to some
		 * functions. The same for some other variables. VZ3 vzquota
		 * internal interface was simple, but now there are some
		 * additional issues: compatibility, new options for example.
		 */
		qf->path = actual_config_file;
		register_cleaner(quota_file_cleaner, qf);
	}
	memset(&qd.stat, 0, sizeof(struct vz_quota_stat));
	free_ugid_quota(&qd.ugid_stat);
	clean_ugid_info(&qd.ugid_stat);

	if (!(option & FL_RELATIVE))
		qd.path = xstrdup(mount_point);
	qd.head.flags = 0;
	set_quotas_from_line(&qd);

	calc_current_usage(&qd);
	check_limits(&qd, 0);

	/* we must read and write whole files cause of checksum */
	if (write_quota_file(fd, &qd, IOF_ALL) < 0)
		exit_cleanup(EC_QUOTAFILE);
#ifdef _DEBUG
	print_status(&qd);
	print_ugid_status(&qd);
#endif
	free_quota_data(&qd);
	close_quota_file(fd);
	debug(LOG_INFO, "quota initialization for id %d complete\n", quota_id);
}

static void quota_on()
{
	int fd;
	struct qf_data qd;
	struct vz_quota_ugid_stat old_conf;
	int rescan = 0;
	int rc;
	struct qf_data *qd_temp;

	init_quota_data(&qd);

	fd = open_quota_file(quota_id, config_file, O_RDWR);
	if (fd < 0 && errno == ENOENT)
		exit(EC_NOQUOTAFILE);
	/* we must read and write whole files cause of checksum */
	if (fd < 0
	    || check_quota_file(fd) < 0
	    || read_quota_file(fd, &qd, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);

	if ((option & FL_PATH) && qd.path && strcmp(qd.path, mount_point)) {
		if (qd.path_len) {
			/*
			 * Change mount point only if it's not relative
			 */
			if (qd.path) free(qd.path);
			qd.path = xstrdup(mount_point);
		}
		option |= FL_FORCE;
	}

	/* set ugid config before scan */
	memcpy(&old_conf, &(qd.ugid_stat.info.config), sizeof(struct vz_quota_ugid_stat));
	set_quotas_from_line(&qd);

	/* clean ugids if user/group quota is off or ugid limit = 0,
	 * saving ugid limit */
	if ( !(qd.head.flags & QUOTA_UGID_ON) ||
	//if ( !(qd.ugid_stat.info.config.flags & VZDQUG_ON) ||
			!qd.ugid_stat.info.config.limit) {

		//unsigned int fl = qd.ugid_stat.info.config.flags;
		unsigned int ul = qd.ugid_stat.info.config.limit;
		clean_ugid_info(&qd.ugid_stat);
		free_ugid_quota(&qd.ugid_stat);
		//qd.ugid_stat.info.config.flags = fl & ~VZDQUG_ON;
		qd.ugid_stat.info.config.limit = ul;
	}

	qd_temp = (struct qf_data*) xmalloc(sizeof(struct qf_data));
	rc = quota_syscall_stat(qd_temp, 1);
	free(qd_temp);
	if (rc >= 0)
		/* quota is on*/
		goto out_run;

	if ((qd.head.flags & QUOTA_ON) && !(option & FL_NOCHECK)) {
		debug(LOG_WARNING, "Incorrect quota shutdown for id %d, "
			"recalculating disk usage\n", quota_id);
		rescan = 1;
	}

	if ((qd.head.flags & QUOTA_DIRTY) && !rescan && !(option & FL_NOCHECK))
	{
		debug(LOG_WARNING, "quota usage is invalid for id %d, "
			"recalculating disk usage...\n", quota_id);
		rescan = 1;
	}

	if (option & FL_FORCE && !rescan)
	{
		debug(LOG_INFO, "Force quota checking for id %d...\n", quota_id);
		rescan = 1;
	}

	/* scan quota after applying cmd-line parameters; they would switch
	 * ugid quota on/off, that acts on calc_current_usage() */
	if (rescan)
		calc_current_usage(&qd);

	check_limits(&qd, 0);
	correct_grace(&qd.stat, &qd.ugid_stat);
	reset_ugid_flags(&qd.ugid_stat, UGID_ALL); /* clear all ugid flags*/

	if (quota_syscall_on(&qd) < 0)
		/* quota is on */
		goto out_run;

	qd.head.flags |= QUOTA_ON;

	if (is_ugid_dirty(&qd.ugid_stat))
		qd.head.flags |= QUOTA_DIRTY;
	else
		qd.head.flags &= ~QUOTA_DIRTY;

	/* we must read and write whole files cause of checksum */
	if (write_quota_file(fd, &qd, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);
#ifdef _DEBUG
	print_status(&qd);
	print_ugid_status(&qd);
#endif
	free_quota_data(&qd);

	close_quota_file(fd);
	debug(LOG_INFO, "Quota was switched on for id %d\n", quota_id);
	return;

out_run:
	debug(LOG_WARNING, "Quota is running for id %d already\n", quota_id);
	exit(EC_RUN);
}

static void quota_off()
{
	int fd;
	int rc;
	struct qf_data qd;
	int force = 0;

	init_quota_data(&qd);

	fd = open_quota_file(quota_id, config_file, O_RDWR);
	/* we must read and write whole files cause of checksum */
	if (fd < 0
	    || check_quota_file(fd) < 0
	    || read_quota_file(fd, &qd, IOF_ALL) < 0) {
		/* file read error */
		if (option & FL_FORCE) {
			debug(LOG_WARNING, "Force switching quota off for id %d "
				"(quota file may be inconsistent)\n", quota_id);
			force = 1;
		} else {
			if (fd < 0 && errno == ENOENT)
				exit(EC_NOQUOTAFILE);
			else
				exit(EC_QUOTAFILE);
		}
	}

	rc = quota_syscall_off(&qd);

	if (rc < 0) {
		/* quota is off or (broken or exceeded) */
		if (errno == EALREADY || errno == EIO) {
			/* quota is broken */
			if (errno == EALREADY)
				debug(LOG_WARNING, "Quota is broken for id %d\n", quota_id);
			else /* quota exceeded */
				debug(LOG_WARNING, "Quota off syscall for id %d: Input/output error.\nQuota exceeded?\n",  quota_id);

			if (force) {
				free_quota_data(&qd);
				debug(LOG_INFO, "Quota was switched off for id %d\n", quota_id);
				return;
			}
			qd.head.flags |= QUOTA_DIRTY;
		} else {
			/* quota is off */
			error(0, 0, "Quota is not running for id %d", quota_id);
			if (force)
				exit(EC_NOTRUN);
			if (!(qd.head.flags & QUOTA_ON))
				exit(EC_NOTRUN);
			else
				debug(LOG_WARNING, "Repairing quota: it was incorrectly"
					" marked as running for id %d\n", quota_id);
		}

	} else {
		/* ok, quota was on */
		if (force) {
			free_quota_data(&qd);
			debug(LOG_INFO, "Quota was switched off for id %d\n", quota_id);
			return;
		}
		if (!(qd.head.flags & QUOTA_ON))
			debug(LOG_WARNING, "Repairing quota: it was incorrectly"
				" marked as stopped for id %d\n", quota_id);
	}

	qd.head.flags &= ~QUOTA_ON;

	/* we must read and write whole files cause of checksum */
	rc = write_quota_file(fd, &qd,
		IOF_ALL);
//		/* write quota stats only if quota was running */
//		(rc < 0) ? IOF_HEAD : IOF_ALL);
	if (rc < 0)
		exit(EC_QUOTAFILE);
	close_quota_file(fd);
#ifdef _DEBUG
	print_status(&qd);
	print_ugid_status(&qd);
#endif

	free_quota_data(&qd);
	debug(LOG_INFO, "Quota was switched off for id %d\n", quota_id);
	return;
}

static void quota_drop()
{
	struct qf_data qd_temp;
	int fd;

	fd = open_quota_file(quota_id, config_file, O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT)
			exit(EC_NOQUOTAFILE);
		else
			exit(EC_QUOTAFILE);
	}

	if (quota_syscall_stat(&qd_temp, 1) >= 0) {
		/* quota is on */
		debug(LOG_WARNING, "Quota is running for id %d\n", quota_id);
		if (option & FL_FORCE)
			debug(LOG_WARNING, "Force quota file delete\n", quota_id);
		else
			exit(EC_RUN);
	}
	if (unlink_quota_file(quota_id, config_file) < 0)
		exit(EC_QUOTAFILE);

	close_quota_file(fd);

	debug(LOG_INFO, "Quota file was deleted for id %d\n", quota_id);
	return;
}

static void quota_reload_ugid()
{
	struct qf_data qd;
	struct qf_data *temp_qd;
	struct ugid_quota *ugid_stat;
	struct dquot *dquot;
	int fd;
	int rc;
	unsigned int i;

	init_quota_data(&qd);

	fd = open_quota_file(quota_id, config_file, O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT)
			exit(EC_NOQUOTAFILE);
		else
			exit(EC_QUOTAFILE);
	}
	/* we must read and write whole files cause of checksum */
	if (check_quota_file(fd) < 0
	    || read_quota_file(fd, &qd, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);

	temp_qd = (struct qf_data *) xmalloc(sizeof(struct qf_data));
	rc = quota_syscall_stat(temp_qd, 0);
	if (rc < 0 || !(temp_qd->ugid_stat.info.config.flags & VZDQUG_ON)) {
		free(temp_qd);
		close_quota_file(fd);
		free_quota_data(&qd);
		return;
	}
	free(temp_qd);

	ugid_stat = &qd.ugid_stat;

	/* set ugid config */
	rc = vzquotactl_ugid_syscall(VZ_DQ_UGID_SETCONFIG, quota_id, 0, 0,
			&(ugid_stat->info.config));
	if (rc < 0)
		error(EC_VZCALL, errno, "Quota ugid_setconfig syscall for id %d", quota_id);

	/* set ugid graces and flags */
	for (i = 0; i < MAXQUOTAS; i++) {
		rc = vzquotactl_ugid_setgrace(&qd, i, &(ugid_stat->info.ugid_info[i]));
		if (rc < 0)
			error(EC_VZCALL, errno, "Quota ugid_setgrace syscall for id %d", quota_id);
	}

	/* set ugid limits */
	reset_dquot_search();
	while( (dquot = get_next_dquot(ugid_stat)) != NODQUOT) {
		struct vz_quota_iface *s;
		struct dq_stat dqstat;

		s = &dquot->obj.istat;
		/* limits */
		memset(&dqstat, 0, sizeof(dqstat));
		dqstat.bsoftlimit = ker2block(s->qi_stat.bsoftlimit);
		dqstat.bhardlimit = ker2block(s->qi_stat.bhardlimit);
		dqstat.isoftlimit = s->qi_stat.isoftlimit;
		dqstat.ihardlimit = s->qi_stat.ihardlimit;
		dqstat.btime = s->qi_stat.btime;
		dqstat.itime = s->qi_stat.itime;
		if (vzquotactl_ugid_setlimit(&qd, s->qi_id, s->qi_type, &dqstat) < 0)
			error(EC_VZCALL, errno, "quotactl failed");
	}

	/* we must read and write whole files cause of checksum */
	rc = write_quota_file(fd, &qd, IOF_ALL);
	if (rc < 0)
		exit(EC_QUOTAFILE);

	close_quota_file(fd);
	free_quota_data(&qd);

	return;
}

static void quota_set()
{
	int fd;
	struct qf_data qd;
	struct qf_data *qd_temp;
	int rc;

	init_quota_data(&qd);

	fd = open_quota_file(quota_id, config_file, O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT)
			exit(EC_NOQUOTAFILE);
		else
			exit(EC_QUOTAFILE);
	}

	if (check_quota_file(fd) < 0)
		exit(EC_QUOTAFILE);

	/* we must read and write whole files cause of checksum */
	if (read_quota_file(fd, &qd, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);
	/* update VE stat from the kernel only, cause we can only change ugid_limit
	 * in ugid_stat; it will be read in quota_syscall_setlimit() from the kernel */
	qd_temp = (struct qf_data *) xmalloc(sizeof(struct qf_data));
	rc = quota_syscall_stat(qd_temp, 1);
	free(qd_temp);
	if (rc < 0) {
		/* debug(LOG_WARNING, "Quota is not running for id %d, "
			"saving new limits to file only\n", quota_id);
		*/
	}

	set_quotas_from_line(&qd);
	check_limits(&qd, 1);
	correct_grace(&qd.stat, NULL);

	if (rc >= 0)
		if (quota_syscall_setlimit(&qd, !(option & FL_LIMITS), !(option & FL_UGIDS)) < 0) {
			if (errno == EBUSY)
				/* EBUSY is returned in case ugid quota is on and limit
				 * was changed: 0 -> x */
				qd.head.flags |= QUOTA_DIRTY;
			else {
				/* debug(LOG_WARNING, "Quota is not running for id %d, "
					"saving new limits to file only\n", quota_id);
				*/
			}
		}

	// Write modified path also
	// this hack was added 'cause migrate need functionality when
	// it changes VE private for given VE (in one partition), so
	// we don't want to recalculate quota and do simple quota file renaming
	if (option & FL_PATH) {
		if (qd.path_len && qd.path) {
			qd.path = xstrdup(mount_point);
			debug(LOG_INFO, "Set mount point path to %s\n",
				mount_point);
		} else
			debug(LOG_INFO, "Mount point path will not be changed, "
				"it's relative\n");
	}

	/* mark quota dirty */
	if (option & FL_FORCE) qd.head.flags |= QUOTA_DIRTY;

	/* we must read and write whole files cause of checksum */
	if (write_quota_file(fd, &qd, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);
#ifdef _DEBUG
	print_status(&qd);
	print_ugid_status(&qd);
#endif
	free_quota_data(&qd);

	close_quota_file(fd);
	debug(LOG_INFO, "Quota limits were set for id %d\n", quota_id);
}

static void quota_setugid(int ugid, struct dq_stat * lim, struct dq_info * info)
{
	int fd;
	struct qf_data data;
	int type;

	init_quota_data(&data);

	fd = open_quota_file(quota_id, config_file, O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT)
			exit(EC_NOQUOTAFILE);
		else
			exit(EC_QUOTAFILE);
	}

	if (check_quota_file(fd) < 0)
		exit(EC_QUOTAFILE);

	if (read_quota_file(fd, &data, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);

	/* Check that 2-level quota is off */

	if (quota_syscall_stat(&data, 0) < 0) {
		/* indicate that quota is off */
		error(EC_NOTRUN, 0, "Quota accounting is off");
	}

	if (!(data.ugid_stat.info.config.flags & VZDQUG_ON)) {
		/* 2-level is off */
		error(EC_NOTRUN, 0, "Second level quota accounting is off");
	}

	type = (option & FL_L2_GROUP) ? GRPQUOTA : USRQUOTA;

	if (info != NULL) {
		data.ugid_stat.info.ugid_info[type] = *info;
		if (vzquotactl_ugid_setgrace(&data, type, info) < 0)
			error(EC_VZCALL, errno, "quotactl failed");
	}

	if (lim != NULL) {
		// Set ugid limits
		struct dquot * dq;
		int dirty;

		dirty = 0;

		/*
		  user enter 1 k block, setugid call get 1 k blocks,
		   another calls get bytes
		 */
		if (vzquotactl_ugid_setlimit(&data, ugid, type, lim) < 0) {
			if (errno == ESRCH) {
				/* ugid entry not found in kernel*/
				dirty = 1;
			} else
				error(EC_VZCALL, errno, "quotactl failed");
		}

		/* add to file */
		dq = lookup_dquot_(&data.ugid_stat, ugid, type);
		if (dq == NODQUOT) {
			dq = add_dquot_(&data.ugid_stat, ugid, type);
			dirty = 1;
			data.ugid_stat.info.buf_size ++;
		}

		/* user enter 1 k block, kernel get bytes */
		lim->bhardlimit = block2ker(lim->bhardlimit);
		lim->bsoftlimit = block2ker(lim->bsoftlimit);

		dq->obj.istat.qi_stat = *lim;

		if (dirty) {
			dq->obj.flags = UGID_DIRTY;
			data.head.flags |= QUOTA_DIRTY;
		}
	}


	/* we must read and write whole files cause of checksum */
	if (write_quota_file(fd, &data, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);

	close_quota_file(fd);
	free_quota_data(&data);
}

int quotainit_proc(int argc, char **argv)
{
	parse_options(argc, argv, quotainit_short_options,
		      quotainit_long_options, quotainit_usage, 0);

	if (!(option & FL_VEID)
	    || !(option & FL_PATH)
	    || ((option & FL_LIMITS) != FL_LIMITS))
		/* if take limits from command string and */
		/* there isn't all limit parameters */
		usage(quotainit_usage);

	quota_init();
	return 0;
}

int quotaon_proc(int argc, char **argv)
{
	parse_options(argc, argv, quotaon_short_options,
		      quotaon_long_options, quotaon_usage, 0);

	if (!(option & FL_VEID))
		usage(quotaon_usage);

	quota_on();
	return 0;
}

int quotaoff_proc(int argc, char **argv)
{
	parse_options(argc, argv, quotaoff_short_options,
		      quotaoff_long_options, quotaoff_usage, 0);

	if (!(option & FL_VEID))
		usage(quotaoff_usage);

	quota_off();
	return 0;
}

int quotaset_proc(int argc, char **argv)
{
	parse_options(argc, argv, quotaset_short_options,
		      quotaset_long_options, quotaset_usage, 0);

	if (!(option & FL_VEID)
	    || (!(option & FL_PATH) && !(option & FL_LIMITS) &&
		    !(option & FL_UGIDS) && !(option & FL_FORCE))
	    )
		usage(quotaset_usage);

	quota_set();
	return 0;
}

int quotaugidset_proc(int argc, char **argv)
{
	int rc;

	rc = parse_options(argc, argv, quotaugidset_short_options,
			   quotaugidset_long_options, quotaugidset_usage, PARSE_SETUGID);
	argc -= (rc+1);
	argv += (rc+1);

	if (!(option & FL_VEID))
		usage(quotaugidset_usage);

	if (option & FL_L2_GRACE) {
		struct dq_info info;

		memset(&info, 0, sizeof(info));
		if (argc != 2
		    || str2time(*argv++, &info.bexpire) < 0
		    || str2time(*argv++, &info.iexpire) < 0)
			usage(quotaugidset_usage);

		quota_setugid(0, NULL, &info);
	} else {
		struct dq_stat stat;
		unsigned int ugid;

		/* get ugid */
		if (argc != 5
		    || str2uint(*argv++, &ugid) < 0)
			usage(quotaugidset_usage);

		/* get limits */
		memset(&stat, 0, sizeof(stat));
		if (str2u64(*argv++, &stat.bsoftlimit) < 0
		    || str2u64(*argv++, &stat.bhardlimit) < 0
		    || str2u32(*argv++, &stat.isoftlimit) < 0
		    || str2u32(*argv++, &stat.ihardlimit) < 0)
			usage(quotaugidset_usage);

		quota_setugid(ugid, &stat, NULL);
	}

	return 0;
}

int quotadrop_proc(int argc, char **argv)
{
	parse_options(argc, argv, quotadrop_short_options,
		      quotadrop_long_options, quotadrop_usage, 0);

	if (!(option & FL_VEID))
		usage(quotadrop_usage);

	quota_drop();
	return 0;
}

int quotareloadugid_proc(int argc, char **argv)
{
	parse_options(argc, argv, quotareloadugid_short_options,
		      quotareloadugid_long_options, quotareloadugid_usage, 0);

	if (!(option & FL_VEID))
		usage(quotareloadugid_usage);

	quota_reload_ugid();
	return 0;
}
