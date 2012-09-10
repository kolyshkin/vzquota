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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <time.h>

#include "vzquota.h"
#include "common.h"


__u32 option;

unsigned int quota_id;
char *mount_point = NULL;
char *config_file = NULL;
char *actual_config_file = NULL;	/* path to actually used config file */

struct vz_quota_stat limits;
//struct vz_quota_stat ugid_limits;
struct vz_quota_ugid_stat ug_config;
//int ugid;

int debug_level = 0;
int batch_mode = 0;

typedef struct clean_item_t {
	func_cleaner_t func;
	void * data;
	struct clean_item_t * next;
} clean_item_t;

static struct clean_item_t * cleaners = NULL;

void register_cleaner(func_cleaner_t func, void * data)
{
	struct clean_item_t * p = xmalloc(sizeof(struct clean_item_t));
	p->func = func;
	p->data = data;
	p->next = cleaners;
	cleaners = p;
}

static void error_cleaning()
{
	struct clean_item_t * p = cleaners;
	while (p) {
		p->func(p->data);
		p = p->next;
	}
}

void vdebug_print(int level, char *oformat, va_list pvar)
{
	FILE* output;

	if (!oformat)
		return;
	if (level <= LOG_WARNING)
		output = stderr;
	else
		output = stdout;
	fprintf(output, VZQUOTA_LOG_PREFIX);
	switch (level) {
		case LOG_ERROR:
			fprintf(output, "(error) "); break;
		case LOG_WARNING:
			fprintf(output, "(warning) "); break;
		case LOG_INFO:
			fprintf(output, "(info) "); break;
		case LOG_DEBUG:
		default:
			fprintf(output, "(debug) "); break;
	}
	vfprintf(output, oformat, pvar);
	return;
}

void debug_print(int level, char *oformat, ...)
{
	va_list pvar;
	va_start(pvar, oformat);
	vdebug_print(level, oformat, pvar);
	va_end(pvar);
}

void exit_cleanup(int status)
{
	error(status, 0, NULL);
}

void error(int status, int errnum, char *oformat, ...)
{
	if (oformat) {
		va_list pvar;

		va_start(pvar, oformat);
		vdebug_print(LOG_ERROR, oformat, pvar);
		va_end(pvar);
		if (errnum)
			fprintf(stderr, ": %s", strerror(errnum));
		fprintf(stderr, "\n");
	}

	if (status) {
		/* do cleaning */
		error_cleaning();
		exit(status);
	}
}

void *xmalloc(size_t size)
{
	void *ptr;
	ptr = malloc(size);
	if (!ptr)
		error(EC_SYSTEM, errno, "not enough memory for %d bytes", size);
	memset(ptr, 0, size);
	return (ptr);
}

void *xrealloc(void *p, size_t size)
{
	void *ptr;
	ptr = realloc(p, size);
	if (!ptr)
		error(EC_SYSTEM, errno, "not enough memory for %d bytes", size);
	return (ptr);
}

char *xstrdup(const char* s)
{
	char *n;
	n = strdup(s);
	if (!n)
		error(EC_SYSTEM, errno,"can't strdup '%s'", s);
	return (n);
}

int str2uint(char *str, unsigned int *number)
{
	char *p;

	ASSERT(str && number);
	*number = strtol(str, &p, 10);
	if (*str == '\0' || *p != '\0')
		return -1;
	return 0;
}

int str2u32(char *str, __u32 * number)
{
	char *p;

	ASSERT(str && number);
	*number = strtol(str, &p, 10);
	if (*str == '\0' || *p != '\0')
		return -1;
	return 0;
}

int str2u64(char *str, __u64 * number)
{
	char *p;

	ASSERT(str && number);
	*number = strtoll(str, &p, 10);
	if (*str == '\0' || *p != '\0')
		return -1;
	return 0;
}

/*
 * run on string - ignore leading spaces and tabs
 */
static char *gostr(char *s)
{
	/* check input params */
	ASSERT(s);

	while (*s && ((' ' == *s) || ('\t' == *s)))
		s++;
	return s;
}

int str_isdigit(char *s)
{
	/* check input params */
	ASSERT(s);

	/* empty string */
	if (0 == strlen(s))
		return -1;

	while (*s) {
		if (!isdigit(*s))
			return -1;
		s++;
	}

	return 0;
}

static int str_istime(char *s, int n)
{

	int i = 0;

	/* check input params */
	ASSERT(s);

	while (*s) {
		if (isdigit(*s) || (':' == *s)) {
			if (':' == *s) {
				i++;
				if (i > n)
					return -1;
			}
			s++;
		} else
			return -1;
	}
	return 0;
}


typedef struct dq_time {
	char c;
	long int t;
} dq_time;

static int dq_thr[4] = {
	1, VE_DQ_MIN, VE_DQ_HOUR, VE_DQ_DAY
};

static dq_time dq_t[] = {
	{'H', VE_DQ_HOUR},
	{'D', VE_DQ_DAY},
	{'W', VE_DQ_WEEK},
	{'M', VE_DQ_MONTH},
	{'Y', VE_DQ_YEAR},
	{0, 0}
};

#define ASP_STR_LEN	256

int str2time(char *s, long int *n)
{
	int len, i, tmp;
	long int val = 0;
	char c, buf[ASP_STR_LEN], *p, *q;

	/* check input params */
	ASSERT((s) && (n));

	if (0 == (len = strlen(s)))
		return -1;
	if (len >= ASP_STR_LEN)
		return -1;

	c = s[len - 1];		// last char (Q: trail spaces?)
	strncpy(buf, s, ASP_STR_LEN);

	if (isdigit(c)) {
		/* format 1 */
		/* ignore leading spaces & tabs */
		p = gostr(buf);

		/* only seconds? */
		if (0 == str_isdigit(p)) {
			if (0 == strlen(p))
				return -1;
			sscanf(p, "%ld", n);
			return 0;
		}

		if (str_istime(p, 3))
			return -1;	/* bad format */

		for (i = 0; i < 4; i++) {
			q = (char *) rindex(p, (int) ':');
			if (q) {
				*q++ = 0;
				/* check - may be empty field */
				if (0 == strlen(q))
					return -1;
				sscanf(q, "%u", &tmp);
			} else {
				/* check - may be empty field */
				if (0 == strlen(p))
					return -1;
				sscanf(p, "%u", &tmp);
			}
			/* convert to seconds */
			tmp *= dq_thr[i];
			if (i < 3) {
				/* check overflow */
				if (tmp >= dq_thr[i + 1])
					return -1;
			}
			val += tmp;
			if (0 == q) {
				*n = val;
				return 0;
			}
		}		/* for */
		*n = val;
	} /* if (isdigit(c)) */
	else {
		/* format 2 */
		/* convert to uppercase */
		c = toupper(c);
		/* reject last char */
		buf[len - 1] = 0;
		/* look up */
		for (i = 0; dq_t[i].c; i++) {
			if (c == dq_t[i].c) {
				/* ignore leading spaces & tabs */
				p = gostr(buf);
				if (str_isdigit(p))
					return -1;
				sscanf(p, "%ld", &val);
				*n = dq_t[i].t * val;
				return 0;
			}
		};
		return -1;	/* bad format */
	}
	return 0;
}

/* Convert time to printable form */
void time2str(time_t seconds, char *buf, int flags)
{
	uint minutes, hours, days;

	minutes = (seconds + 30) / 60;	/* Rounding */
	hours = minutes / 60;
	minutes %= 60;
	days = hours / 24;
	hours %= 24;
	if (flags & TF_ROUND) {
		if (days)
			sprintf(buf, "%ddays", days);
		else
			sprintf(buf, "%02d:%02d", hours, minutes);
	} else {
		if (minutes || (!minutes && !hours && !days))
			sprintf(buf, "%uminutes",
				(uint) (seconds + 30) / 60);
		else if (hours)
			sprintf(buf, "%uhours", hours + days * 24);
		else
			sprintf(buf, "%udays", days);
	}
}

/* For printing grace time */
void difftime2str(time_t seconds, char *buf)
{
	time_t now;
	buf[0] = 0;
	if (!seconds)
		return;
	time(&now);
	if (seconds <= now) {
		strcpy(buf, "none");
		return;
	}
	time2str(seconds - now, buf, TF_ROUND);
}

static char time_usage[] =
"Available time formats:\n"
"1. dd:hh:mm:ss\n"
"2. xxA, where A - h/H(hour);d/D(day);w/W(week);m/M(month);y/Y(year)\n"
"For instance: 7D - 7 days; 01w - 1 week; 3m - 3 months\n";

//TODO realpath ?
char* globolize_path(char *path)
{
	char *cwd = NULL;
	char *newpath = NULL;

	ASSERT(path);

	if (path[0] == '/')
		return xstrdup(path);

	if (!(cwd = getcwd(NULL, 0)))
		error(EC_SYSTEM, errno, "getcwd");

	newpath = xmalloc(strlen(cwd) + strlen(path) + 2);
	sprintf(newpath, "%s/%s", cwd, path);

	xfree(cwd);
	return newpath;
}

int parse_options(int argc, char **argv, char *short_options,
		   struct option *long_options, char *opt_usage, int cmd_type)
{
	int c;

	option = 0;
	memset(&limits, 0, sizeof(limits));
	memset(&ug_config, 0, sizeof(ug_config));

	if (argc < 2)
		/* cmd <veid> */
		usage(opt_usage);

	/* skip 'cmd' */
	argc --;
	argv ++;

	/* get VEID */
	if (str2uint(argv[0], &quota_id) < 0)
		usage(opt_usage);
	option |= FL_VEID;

	optind = 0;

	while ((c =
		getopt_long(argc, argv, short_options, long_options,
			    NULL)) != -1) {
		switch (c) {
		case 13:
			option |= FL_NOCHECK;
			 break;

		case 'r':
			break;

		case 's':
			if (!strcmp(optarg, "1")) {
				ug_config.flags |= VZDQUG_ON;
			} else if (!strcmp(optarg, "0")) {
				ug_config.flags &= ~VZDQUG_ON;
			} else {
				usage(opt_usage);
			}
			option |= FL_SQT;
			break;

		case 'u' :
			if (cmd_type & PARSE_SETUGID) {
				option |= FL_L2_USER;
				break;
			}

			if (str2uint(optarg, &ug_config.limit) < 0)
				usage(opt_usage);
			option |= FL_UGL;
			break;

		case 'g':
			option |= FL_L2_GROUP;
			break;

		case 't':
			if (cmd_type & PARSE_SETUGID)
				option |= FL_L2_GRACE;
			else
				option |= FL_SQT;
			break;

		case 'G':
			option |= FL_DUMP_GRACE;
			break;

		case 'U':
			option |= FL_DUMP_LIMITS;
			break;

		case 'T':
			option |= FL_DUMP_EXPTIME;
			break;
		case 'c':
			option |= FL_CONF_FILE;
			config_file = optarg;
			break;

		case 'F':
			option |= FL_DUMP_LIMITS_FIRST;
			break;

		case 'f':
			option |= FL_FORCE;
			break;

		case 'p':
			mount_point = globolize_path(optarg);
			option |= FL_PATH;
			break;

		case 'b':
			if (str2u64(optarg, &limits.dq_stat.bsoftlimit) < 0)
				usage(opt_usage);
			option |= FL_BSL;
			break;

		case 'B':
			if (str2u64(optarg, &limits.dq_stat.bhardlimit) < 0)
				usage(opt_usage);
			option |= FL_BHL;
			break;

		case 'e':
			if (str2time(optarg, &limits.dq_info.bexpire) < 0)
				usage(time_usage);
			option |= FL_BET;
			break;

		case 'i':
			if (str2u32(optarg, &(limits.dq_stat.isoftlimit)) < 0)
				usage(opt_usage);
			option |= FL_ISL;
			break;

		case 'I':
			if (str2u32(optarg, &(limits.dq_stat.ihardlimit)) < 0)
				usage(opt_usage);
			option |= FL_IHL;
			break;

		case 'n':
			if (str2time(optarg, &limits.dq_info.iexpire) < 0)
				usage(time_usage);
			option |= FL_IET;
			break;

		case 'R':
			option |= FL_RELATIVE;
			break;

		case '?':
		default:
			usage(opt_usage);
		}
	}

	if ((option & FL_RELATIVE) && (option & FL_CONF_FILE))
		error(EC_USAGE, 0, "-c and -R options can't be set together\n");

	return optind;
}

static void print_version()
{
	printf("Vzquota version %s\n", VZQUOTA_VERSION);
	exit(EC_SUCCESS);
}

/*pointers to argc and argv of main()*/
void parse_global_options(int *argc, char ***argv, const char *usg)
{
	int c;
	static char short_options[] = "+hvqVb";
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'V'},
		{0, 0, 0, 0}
	};
	int option_index = 0;
	int help = 0;		/* Has the user asked for help?  This
				   lets us support the `vzquota -H cmd'
				   convention to give help for cmd. */
#ifndef _DEBUG
	debug_level = LOG_WARNING;
#else
	debug_level = LOG_DEBUG;
#endif
	while ((c =
		getopt_long(*argc, *argv, short_options, long_options,
			    &option_index)) != -1) {
		switch (c) {
		case 'h':
			help = 1;
			break;
		case 'V':
			print_version();
			break;
		case 'v':
			debug_level++;
			break;
		case 'b':
			batch_mode++;
			break;
		case 'q':
			debug_level = LOG_ERROR;
			break;
		case '?':
		default:
			usage(usg);
		}
	}
	*argc -= optind;
	*argv += optind;

	if (*argc < 1)
		usage(usg);

	if (help)
		*argc = -1;
}

void usage(const char *usg)
{
	if (!command_name)
		fprintf(stderr, usg, program_name);
	else
		fprintf(stderr, usg, program_name, command_name);
	exit(EC_USAGE);
}

