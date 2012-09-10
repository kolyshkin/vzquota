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

#include "common.h"
#include "vzquota.h"
#include "quota_io.h"
#include "quotadump.h"

const char *program_name = "vzdqdump";
char *command_name = NULL; /* for usage() */

static char dump_usage[] =
"Usage: %s quotaid [-c file] [-f] commands\n"
"Dumps user/group quota information from quota file.\n"
"\t-c file\tuse given quota file\n"
"\t-f\tdump data from kernel rather than quota file\n"
"Commands specify what user/group information to dump:\n"
"\t-F\tfirst level quota\n"
"\t-G\tgrace time\n"
"\t-U\tdisk limits\n"
"\t-T\texpiration times\n"
;

static char dump_short_options[] = "c:fFGUT";
static struct option dump_long_options[] = {
	{"quota-file", required_argument, NULL, 'c'},
	{"first", no_argument, NULL, 'F'},
	{"kernel", no_argument, NULL, 'f'},
	{"gracetime", no_argument, NULL, 'G'},
	{"limits", no_argument, NULL, 'U'},
	{"exptimes", no_argument, NULL, 'T'},
	{0, 0, 0, 0}
};

int main(int argc, char **argv)
{
	int fd = 0;
	unsigned int i;
	struct qf_data qd;
	struct ugid_quota *q = NULL;
	unsigned int ugid_quota_status = 0;

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

	if (!(option & FL_FORCE)) {

		/* dump from quota file */
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
		close_quota_file(fd);
		ugid_quota_status =
			(qd.head.flags & QUOTA_UGID_ON) ? 1 : 0;
	} else {

		/* dump from kernel */
		if (quota_syscall_stat(&qd, 0) < 0)
			error(EC_NOTRUN, 0, "Quota accounting is off");
		ugid_quota_status =
			(qd.ugid_stat.info.config.flags & VZDQUG_ON) ? 1 : 0;
	}

	/* status of user/group quota */
	printf("%s\t%u\n", STATUS_LABEL, ugid_quota_status);

	if (!ugid_quota_status)
		goto print_first_level;

	q = &qd.ugid_stat;

	/* grace times */
	if (option & FL_DUMP_GRACE) {
		printf("#%s\ttype\tblock\tinode\n", GRACE_LABEL);
		for (i = 0; i < MAXQUOTAS; i++) {
			printf("%s\t%u\t%lu\t%lu\n",
				GRACE_LABEL,
				i,
				q->info.ugid_info[i].bexpire,
				q->info.ugid_info[i].iexpire
			);
		}
	}

	if (option & (FL_DUMP_LIMITS | FL_DUMP_EXPTIME)) {
		/* number of ugids */
		printf("%s\t%u\n", NUMBER_LABEL, q->dquot_size);

		if (q->dquot_size) {
			struct dquot *dq;
			struct dquot *obj[q->dquot_size];
			struct vz_quota_iface *s;

			printf("#%s\tID\ttype", UGID_LABEL);
			if (option & FL_DUMP_LIMITS)
				printf("\tbsoft\tbhard\tisoft\tihard");
			if (option & FL_DUMP_EXPTIME)
				printf("\tbtime\titime");
			printf("\n");

			sort_dquot(q, obj);
			for (i = 0; i < q->dquot_size; i++) {
				dq = obj[i];
				s = &(dq->obj.istat);

				/* id, type */
				printf("%s\t%u\t%u",
					UGID_LABEL,
					s->qi_id,
					s->qi_type);

				/* limits */
				if (option & FL_DUMP_LIMITS)
					printf("\t%llu\t%llu\t%u\t%u",
						ker2block(s->qi_stat.bsoftlimit),
						ker2block(s->qi_stat.bhardlimit),
						s->qi_stat.isoftlimit,
						s->qi_stat.ihardlimit);

				/* exp times */
				if (option & FL_DUMP_EXPTIME)
					printf("\t%lu\t%lu",
						(s->qi_stat.bcurrent < s->qi_stat.bsoftlimit) ?
							0 : s->qi_stat.btime,
						(s->qi_stat.icurrent < s->qi_stat.isoftlimit) ?
							0 : s->qi_stat.itime
					);
				printf("\n");
			}
		}
	}

print_first_level:
	if (option & FL_DUMP_LIMITS_FIRST) {
		struct vz_quota_stat *stat = &qd.stat;

		printf(FIRST_LEVEL_LABEL "\n");

		/* usage soft hard grace expire */
		printf("%llu %llu %llu %lu %lu\n",
		      stat->dq_stat.bcurrent, stat->dq_stat.bsoftlimit,
		      stat->dq_stat.bhardlimit, stat->dq_stat.btime,
		      stat->dq_info.bexpire);

		/* usage soft hard grace expire */
		printf("%u %u %u %lu %lu\n",
		      stat->dq_stat.icurrent, stat->dq_stat.isoftlimit,
		      stat->dq_stat.ihardlimit, stat->dq_stat.itime,
		      stat->dq_info.iexpire);
	}

	free_quota_data(&qd);

	return EC_SUCCESS;
}
