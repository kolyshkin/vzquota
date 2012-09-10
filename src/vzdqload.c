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

#include <sys/file.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "vzquota.h"
#include "quota_io.h"
#include "quotadump.h"

const char *program_name = "vzdqload";
char *command_name = NULL; /* for usage() */

static char dump_usage[] =
"Usage: %s quotaid [-c file] commands\n"
"Loads user/group quota information from stdin into quota file.\n"
"\t-c file\tuse given quota file\n"
"Commands specify what user/group information to load:\n"
"\t-F\tfirst level quota\n"
"\t-G\tgrace time\n"
"\t-U\tdisk limits\n"
"\t-T\texpiration times\n"
;

static char dump_short_options[] = "c:FGUT";
static struct option dump_long_options[] = {
	{"first", no_argument, NULL, 'F'},
	{"quota-file", required_argument, NULL, 'c'},
	{"gracetime", no_argument, NULL, 'G'},
	{"limits", no_argument, NULL, 'U'},
	{"exptimes", no_argument, NULL, 'T'},
	{0, 0, 0, 0}
};

void err(char *line, struct qf_data *q, int fd)
{
	if (q) free_quota_data(q);
	if (fd > 0) close_quota_file(fd);
	error(EC_USAGE, 0, "Invalid input data: %s", line);
}

/* reads _argnum_ arguments from _f_ excluding comments # and empty lines;
 * returns: 0 - end of file, -1 - invalid syntax */
int read_line(FILE *f, char *line, size_t bufsize,
		int argnum, const char *fmt, ...)
{
	int i;
	va_list pvar;

	memset(line, 0, bufsize);
	do if (! fgets(line, bufsize-1, f)) {
		/* end of file */
		error(0, 0, "End of input file");
		return 0;
	}
	while (line[0] == 0 || line[0] == '#');

	va_start(pvar, fmt);
	i = vsscanf(line, fmt, pvar);
	va_end(pvar);
	if (argnum != i)
		/* invalid syntax */
		return -1;
	return i;
}

int main(int argc, char **argv)
{
	int fd = 0;
	FILE *file = stdin;
	int rc;

	struct qf_data qd;
	struct ugid_quota *q = NULL;
	unsigned int ugid_quota_status = 0;
	unsigned int ugidnum = 0;

	size_t bufsize = 4*1024;
	char line[bufsize];
	char label[bufsize];
	unsigned int i;

	parse_global_options(&argc, &argv, dump_usage);
	argc += 1;
	argv -= 1;
	parse_options(argc, argv, dump_short_options,
		dump_long_options, dump_usage, 0);

	if (!(option & FL_VEID))
		usage(dump_usage);
	if (!(option & FL_DUMP))
		usage(dump_usage);

	init_quota_data(&qd);

	/* open quota file */
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

	/* status of user/group quota */
	rc = read_line(file, line, bufsize, 2, "%s%u", &label, &ugid_quota_status);
	if (rc == 0
			|| rc < 0
			|| strcmp(label, STATUS_LABEL)
			|| ugid_quota_status > 1)
		err(line, &qd, fd);

	if (!ugid_quota_status) {
		/* ugid quota is off */
//		qd.head.flags &= ~QUOTA_UGID_ON;

		/* we must read and write whole files cause of checksum */
//		rc = write_quota_file(fd, &qd, IOF_ALL);
//		if (rc < 0)
//			exit(EC_QUOTAFILE);

		goto get_first_level;
	}

	/* ugid quota is on */
	q = &qd.ugid_stat;
//	qd.head.flags |= QUOTA_UGID_ON;

	/* grace times */
	if (option & FL_DUMP_GRACE) {
		for (i = 0; i < MAXQUOTAS; i++) {
			unsigned int type;
			unsigned long bexp, iexp;
			rc = read_line(file, line, bufsize, 4, "%s%u%lu%lu",
					&label, &type, &bexp, &iexp);
			if( rc == 0
					|| rc < 0
					|| strcmp(label, GRACE_LABEL)
					|| type != i)
				err(line, &qd, fd);
			q->info.ugid_info[i].bexpire = (time_t) bexp;
			q->info.ugid_info[i].iexpire = (time_t) iexp;

		}
	}

	/* number of ugids */
	if (option & (FL_DUMP_LIMITS | FL_DUMP_EXPTIME)) {
		rc = read_line(file, line, bufsize, 2, "%s%u", &label, &ugidnum);
		if (rc == 0
				|| rc < 0
				|| strcmp(label, NUMBER_LABEL))
			err(line, &qd, fd);
	}

	/* ugid objs */
	if ((option & (FL_DUMP_LIMITS | FL_DUMP_EXPTIME)) && ugidnum) {

		for (i = 0; i < ugidnum; i++) {
			unsigned int id, type;
			unsigned int bsoft, bhard, isoft, ihard;
			unsigned long btime, itime;
			struct dquot *dq = NODQUOT;
			struct vz_quota_iface *s;

			if (! (option & FL_DUMP_EXPTIME))
				/* limits only */
				rc = read_line(file, line, bufsize, 7, "%s%u%u%u%u%u%u",
					&label, &id, &type,
					&bsoft, &bhard, &isoft, &ihard);
			else if (! (option & FL_DUMP_LIMITS))
				/* grace times only */
				rc = read_line(file, line, bufsize, 5, "%s%u%u%lu%lu",
					&label, &id, &type,
					&btime, &itime);
			else
				/* both */
				rc = read_line(file, line, bufsize, 9, "%s%u%u%u%u%u%u%lu%lu",
					&label, &id, &type,
					&bsoft, &bhard, &isoft, &ihard,
					&btime, &itime);
			if( rc == 0
					|| rc < 0
					|| strcmp(label, UGID_LABEL)
					|| type > MAXQUOTAS)
				err(line, &qd, fd);

			/* find or create obj */
			dq = lookup_dquot_(q, id, type);
			if (dq == NODQUOT)
				dq = add_dquot_(q, id, type);
			s = &dq->obj.istat;

			/* limits */
			if (option & FL_DUMP_LIMITS) {
				s->qi_stat.bsoftlimit = block2ker(bsoft);
				s->qi_stat.bhardlimit = block2ker(bhard);
				s->qi_stat.isoftlimit = isoft;
				s->qi_stat.ihardlimit = ihard;
			}

			/* exp times */
			if (option & FL_DUMP_EXPTIME) {
				s->qi_stat.btime = (time_t) btime;
				s->qi_stat.itime = (time_t) itime;
			}
		}
		/* set number of ugid objects in buffer */
		q->info.buf_size = q->dquot_size;
	}

get_first_level:
	if (option & FL_DUMP_LIMITS_FIRST) {
		unsigned long long bcurrent, bsoftlimit, bhardlimit;
		unsigned long btime, bexpire;
		unsigned icurrent, isoftlimit, ihardlimit;
		unsigned long itime, iexpire;
		struct vz_quota_stat *stat = &qd.stat;

		rc = read_line(file, line, bufsize, 1, "%s", &label);
		if (rc < 0 || rc == 0 || strncmp(label, FIRST_LEVEL_LABEL,
			strlen(FIRST_LEVEL_LABEL)) != 0) {
			fprintf(stderr, "Invalid input data: %s\n", line);
			exit(EC_USAGE);
		}

		/* OK, read 1k-blocks */
		rc = read_line(file, line, bufsize, 5, "%llu%llu%llu%lu%lu",
			&bcurrent, &bsoftlimit, &bhardlimit, &btime, &bexpire);
		if (rc < 0 || rc == 0) {
			fprintf(stderr, "Invalid input data: %s\n", line);
			exit(EC_USAGE);
		}

		/* read inodes */
		rc = read_line(file, line, bufsize, 5, "%u%u%u%lu%lu",
			&icurrent, &isoftlimit, &ihardlimit, &itime, &iexpire);
		if (rc < 0 || rc == 0) {
			fprintf(stderr, "Invalid input data: %s\n", line);
			exit(EC_USAGE);
		}

		stat->dq_stat.bhardlimit = bhardlimit;
		stat->dq_stat.bsoftlimit = bsoftlimit;
		stat->dq_stat.bcurrent = bcurrent;
		stat->dq_stat.btime = btime;
		stat->dq_info.bexpire = bexpire;
		stat->dq_stat.ihardlimit = ihardlimit;
		stat->dq_stat.isoftlimit = isoftlimit;
		stat->dq_stat.icurrent = icurrent;
		stat->dq_stat.itime = itime;
		stat->dq_info.iexpire = iexpire;
	}

	if (ugid_quota_status == 1 || option & FL_DUMP_LIMITS_FIRST) {
		/* we must read and write whole files cause of checksum */
		rc = write_quota_file(fd, &qd, IOF_ALL);
		if (rc < 0)
			exit(EC_QUOTAFILE);
	}

	close_quota_file(fd);
	free_quota_data(&qd);

	return EC_SUCCESS;
}
