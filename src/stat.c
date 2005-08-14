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

#include <time.h>
#include <fcntl.h>

#include "vzquota.h"
#include "common.h"
#include "quota_io.h"


/* quota stat, quota show */

#ifndef L2

static char vestat_usage[] =
"Usage: %s %s quotaid\n"
"\t(Specify the --help global option for a list of other help options)\n";

static char quotashow_usage[] =
"Usage: %s %s quotaid [-c file]\n"
"\t(Specify the --help global option for a list of other help options)\n";

static char vestat_short_options[] = "-c:";
static struct option vestat_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{0, 0, 0, 0}
};

#else//L2

static char vestat_usage[] =
"Usage: %s %s quotaid [-f] [-c file] [-t]\n"
"\t(Specify the --help global option for a list of other help options)\n";

static char quotashow_usage[] =
"Usage: %s %s quotaid [-c file] [-t]\n"
"\t(Specify the --help global option for a list of other help options)\n";

static char vestat_short_options[] = "-c:tf";
static struct option vestat_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{"user-group", no_argument, NULL, 't'},
	{0, 0, 0, 0}
};
	
static char quotashow_short_options[] = "-c:t";
static struct option quotashow_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{"user-group", no_argument, NULL, 't'},
	{0, 0, 0, 0}
};

#endif //L2


/* For printing '*' char when overlimit */

#ifndef L2
char overlim(__u64 usage, __u64 softlim, __u64 hardlim)
{
	if ((softlim && usage > softlim) || (hardlim && usage > hardlim))
		return '*';
	return ' ';
}
#else
char b_overlim(qint usage, qint softlim, qint hardlim)
{
	if ((softlim && usage > softlim) || (hardlim && usage > hardlim))
		return '*';
	return ' ';
}

char i_overlim(__u32 usage, __u32 softlim, __u32 hardlim)
{
	if ((softlim && usage > softlim) || (hardlim && usage > hardlim))
		return '*';
	return ' ';
}
#endif

#ifndef L2
static void print_status(struct vz_quota_stat* qstat)
{
	char buf[QMAXTIMELEN];

	printf("%11s %14s  %14s %14s %8s\n",
		"resource", "usage", "softlimit", "hardlimit", "grace");

	if (qstat->bcurrent <= qstat->bsoftlimit)
		qstat->btime = 0;

	difftime2str(qstat->btime, buf);
	printf("%11s %14u%c %14u %14u %8s\n",
		"1k-blocks", block_view(qstat->bcurrent),
		overlim(qstat->bcurrent, qstat->bsoftlimit, qstat->bhardlimit),
		block_view(qstat->bsoftlimit), block_view(qstat->bhardlimit), buf);
	
	if (qstat->icurrent <= qstat->isoftlimit)
		qstat->itime = 0;

	difftime2str(qstat->itime, buf);
	printf("%11s %14u%c %14u %14u %8s\n",
		"inodes", qstat->icurrent,
		overlim(qstat->icurrent, qstat->isoftlimit, qstat->ihardlimit),
		qstat->isoftlimit, qstat->ihardlimit, buf);
}
#else
void print_status(struct qf_data *qd)
{
	char buf[QMAXTIMELEN];
	struct vz_quota_stat *stat = &qd->stat;
	struct dq_stat *s = &(stat->dq_stat);
	struct dq_info *i = &(stat->dq_info);

	if (!batch_mode)
		printf("%11s %14s  %14s %14s %8s\n",
		       "resource", "usage", "softlimit", "hardlimit", "grace");

	if (s->bcurrent <= s->bsoftlimit)
		s->btime = 0;

	if (batch_mode) {
		/* usage soft hard grace expire */
		printf("%14u %14u %14u %14lu %14lu\n",
		      ker2block(s->bcurrent), ker2block(s->bsoftlimit), ker2block(s->bhardlimit),
		      s->btime, i->bexpire);
	} else {
		difftime2str(s->btime, buf);
		printf("%11s %14u%c %14u %14u %8s\n",
		       "1k-blocks", ker2block(s->bcurrent),
		       b_overlim(s->bcurrent, s->bsoftlimit, s->bhardlimit),
		       ker2block(s->bsoftlimit), ker2block(s->bhardlimit), buf);
	}
	
	if (s->icurrent <= s->isoftlimit)
		s->itime = 0;

	if (batch_mode) {
		/* usage soft hard grace expire */
		printf("%14u %14u %14u %14lu %14lu\n",
		      s->icurrent, s->isoftlimit, s->ihardlimit,
		      s->itime, i->iexpire);
	} else {	
		difftime2str(s->itime, buf);
		printf("%11s %14u%c %14u %14u %8s\n",
		       "inodes", s->icurrent,
		       i_overlim(s->icurrent, s->isoftlimit, s->ihardlimit),
		       s->isoftlimit, s->ihardlimit, buf);
	}
}

void print_ugid_status(struct qf_data *qd)
{
	char buf1[QMAXTIMELEN], buf2[QMAXTIMELEN];
	struct ugid_quota *q = &qd->ugid_stat;
	struct dquot *dq;
	struct dquot *obj[q->dquot_size];
	unsigned int i;

	/* ugid quota status */
	/* if force option was supplied, we do not read quota file and thus
	 * do not know 2-level quota status at the next run */
	if (batch_mode) {
		printf("%d %d\n", (option & FL_FORCE) ? -1
		       : (qd->head.flags & QUOTA_UGID_ON) ? 1 : 0, (q->info.config.flags & VZDQUG_ON) ? 1 : 0);
		/* ugid usage */		
		printf("%d %d %d\n", q->info.config.count, q->info.buf_size, q->info.config.limit);
		printf("%d\n", (q->info.config.flags & VZDQUG_FIXED_SET) ? 1 : 0);
	} else {
		printf("%s %s,%s\n", "User/group quota:",
		       (option & FL_FORCE) ? "-" :
		       (qd->head.flags & QUOTA_UGID_ON) ? "on" : "off",
		       (q->info.config.flags & VZDQUG_ON) ? "active" : "inactive");
		/* ugid usage */
		printf("%s%s loaded %u, total %u, limit %u\n",
		       (q->info.buf_size > q->info.config.limit) ? "*" : "",
		       "Ugids:",
		       q->info.config.count,
		       q->info.buf_size,
		       q->info.config.limit);
		printf("%s %s\n", "Ugid limit was exceeded:",
		       (q->info.config.flags & VZDQUG_FIXED_SET) ? "yes" : "no");
	}

	/* grace times */
	if (!batch_mode) {
		printf("\n");
		printf("User/group grace times and quotafile flags:\n");
		printf("%5s %14s %14s %10s\n", "type", "block_exp_time", "inode_exp_time", "dqi_flags");
	}
	for (i = 0; i < MAXQUOTAS; i++) {
		if (batch_mode) {
			printf("%5s %ld %ld %9Xh\n",
			       type2name(i), q->info.ugid_info[i].bexpire,
			       q->info.ugid_info[i].iexpire, q->info.ugid_info[i].flags);
			continue;
		}
		buf1[0] = buf2[0] = 0;
		if (q->info.ugid_info[i].bexpire)
			time2str(q->info.ugid_info[i].bexpire, buf1, TF_ROUND);
		if (q->info.ugid_info[i].iexpire)
			time2str(q->info.ugid_info[i].iexpire, buf2, TF_ROUND);
		printf("%5s %14s %14s %9Xh\n",
			type2name(i), buf1, buf2, q->info.ugid_info[i].flags);
	}

	if (!q->dquot_size) return;

	sort_dquot(q, obj);
		
	/* output ugid objects */
	if (!batch_mode) {
		printf("\n");
		printf("User/group objects:\n");
		printf("%-11s %5s %9s %11s %11s %11s %8s %6s\n",
		       "ID", "type", "resource", "usage", "softlimit", "hardlimit", "grace", "status");
	}
	for (i = 0; i < q->dquot_size; i++) {
		struct vz_quota_iface *s;
		time_t t;
		char status[256] = "";
	 
	   	dq = obj[i];
		s = &(dq->obj.istat);
		
		/* status */
		if (batch_mode) {
			sprintf(status, "%d %d", (dq->obj.flags & UGID_LOADED) ? 1 : 0,
				(dq->obj.flags & UGID_DIRTY) ? 1 : 0);
		} else {
			if (dq->obj.flags & UGID_LOADED)
				sprintf(status, "loaded");
			if (dq->obj.flags & UGID_DIRTY)
				sprintf(status, "%s%sdirty", status, (strlen(status)) ? "," : "");
		}

	  	/* blocks */	
		t = (s->qi_stat.bcurrent < s->qi_stat.bsoftlimit) ? 0 : s->qi_stat.btime;
		if (batch_mode) {
			sprintf(buf1, "%ld", t);
		} else {
			difftime2str(t, buf1);
		}
		printf("%-11u %5s %9s %11u %11u %11u %8s %6s\n",
			s->qi_id, type2name(s->qi_type), "1k-blocks",
			ker2block(s->qi_stat.bcurrent),
			ker2block(s->qi_stat.bsoftlimit),
			ker2block(s->qi_stat.bhardlimit),
			buf1, status);

		/* inodes */
		t = (s->qi_stat.icurrent < s->qi_stat.isoftlimit) ? 0 : s->qi_stat.itime;
		if (batch_mode) {
			sprintf(buf1, "%ld", t);
		} else {
			difftime2str(t, buf1);
		}
		printf("%-11u %5s %9s %11u %11u %11u %8s %6s\n",
			s->qi_id, type2name(s->qi_type), "inodes",
			s->qi_stat.icurrent, s->qi_stat.isoftlimit, s->qi_stat.ihardlimit,
			buf1, status);
	}
}
#endif

int vestat_proc(int argc, char **argv)
{
	int fd = 0;
	int rc = 0;
#ifndef L2
	struct vz_quota_stat qstat;
#else
	struct qf_data qd;

	init_quota_data(&qd);
#endif
	parse_options(argc, argv, vestat_short_options,
		      vestat_long_options, vestat_usage, 0);

	if (!(option & FL_VEID))
		usage(vestat_usage);

#ifdef L2
	/* if force option was supplied, we do not read and update quota file */
	if (!(option & FL_FORCE)) {
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
	}
#endif

#ifndef L2
	if (vzquotactl_syscall(VE_QUOTA_GETSTAT, quota_id, &qstat, NULL) < 0) {
		debug(LOG_WARNING, "Quota accounting is off, "
			"try vzquota show id\n");
		exit(EC_NOTRUN);
	} else {
		fd = open_quota_file(quota_id, config_file, O_RDWR);
		if (fd < 0
		    || check_quota_file(fd) < 0
		    || write_quota_file(fd, NULL, &qstat, NULL) < 0)
			exit(EC_QUOTAFILE);
		close_quota_file(fd);
	}
#else
	if (quota_syscall_stat(&qd, !(option & FL_SQT)) < 0) {
		/* indicate that quota is off */
		error(EC_NOTRUN, 0, "Quota accounting is off, "
			"try vzquota show id\n");

	} else if (!(option & FL_FORCE)) {
	   	if (write_quota_file(fd, &qd, IOF_ALL) < 0)
			exit(EC_QUOTAFILE);
		close_quota_file(fd);
	}
#endif

#ifndef L2
	print_status(&qstat);
#else
	print_status(&qd);
	if (option & FL_SQT) {
		print_ugid_status(&qd);

		/* indicate that VE quota is on but user/group quota is inactive in KERNEL */
		if (!(qd.ugid_stat.info.config.flags & VZDQUG_ON))
			rc = EC_UG_NOTRUN;
	}
	
	free_quota_data(&qd);
#endif

	/* indicate that VE and user/group quotas are on */
	return rc;
}

int quotashow_proc(int argc, char **argv)
{
	int fd;
	int rc = 0;
#ifndef L2
	struct vz_quota_stat qstat;
	struct vz_quota_header head;
#else
	struct qf_data qd;

	init_quota_data(&qd);
#endif
	parse_options(argc, argv, quotashow_short_options,
		      quotashow_long_options, quotashow_usage, 0);

	if (!((option & FL_VEID) || (option & FL_CONF_FILE)))
		usage(quotashow_usage);

	fd = open_quota_file(quota_id, config_file, O_RDWR);
#ifndef L2
	if (fd < 0)
		error(EC_QUOTAFILE, 0, 
				"Quota file must exist for show command");
	if (check_quota_file(fd) < 0
	    || read_quota_file(fd, &head, &qstat, NULL, 0) < 0)
		exit(EC_QUOTAFILE);
#else
	if (fd < 0) {
		if (errno == ENOENT)
			error(EC_NOQUOTAFILE, 0, "Quota file must exist for show command");
		else
			exit(EC_QUOTAFILE);
	}


	/* we must read and write whole files cause of checksum */
	if (check_quota_file(fd) < 0
	    || read_quota_file(fd, &qd, IOF_ALL) < 0)
		exit(EC_QUOTAFILE);
#endif

#ifndef L2
	if (head.flags & QUOTA_ON)
#else
	if (qd.head.flags & QUOTA_ON)
#endif
		debug(LOG_WARNING, "Quota is running, so data reported from "
			"quota file may not reflect current values\n");

#ifdef L2
	if (qd.head.flags & QUOTA_DIRTY)
		rc = EC_QUOTADIRTY;
#endif

#ifndef L2
	print_status(&qstat);
#else
	print_status(&qd);
	if (option & FL_SQT)
		print_ugid_status(&qd);

	free_quota_data(&qd);
#endif
	close_quota_file(fd);
	return rc;
}
