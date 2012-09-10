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

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdarg.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <asm/types.h>

#include "vzquota.h"

/* current utilities version */
#define VZQUOTA_VERSION	"2.5.0"

#define FL_VEID		0x0000001
#define FL_PATH		0x0000002
#define FL_FORCE	0x0000004
/*#define FL_FROM_LINE	0x0000008*/
#define FL_CONF_FILE	0x0000010

#define FL_BSL		0x0000100
#define FL_BHL		0x0000200
#define FL_BET		0x0000400

#define FL_ISL		0x0001000
#define FL_IHL		0x0002000
#define FL_IET		0x0004000

#define FL_RSQ		0x0010000 /* FL_RSQ is depricated */
#define FL_SQT		0x0020000
#define FL_UGL		0x0040000

/* this is for dump utility*/
#define FL_DUMP_GRACE	0x0100000
#define FL_DUMP_LIMITS	0x0200000
#define FL_DUMP_EXPTIME	0x0400000
#define FL_DUMP_LIMITS_FIRST	0x0800000
/* this is for setlimit */
#define FL_L2_USER	0x0100000	/* set limits for user */
#define FL_L2_GROUP	0x0200000	/* set limits for group */
#define FL_L2_GRACE	0x0400000	/* set graces limits */

#define FL_NOCHECK	0x1000000
/* VZ4 scheme: quota config file path is relative to quota accounting point */
#define FL_RELATIVE	0x2000000

#define FL_L2_UGID	(FL_L2_USER | FL_L2_GROUP)

#define FL_LIMITS	(FL_BSL | FL_BHL | FL_BET | FL_ISL | FL_IHL | FL_IET)
#define FL_UGIDS	(FL_SQT | FL_UGL)
#define FL_DUMP		(FL_DUMP_GRACE | FL_DUMP_LIMITS | FL_DUMP_EXPTIME | FL_DUMP_LIMITS_FIRST)


extern __u32 option;


extern unsigned int quota_id;
extern char *mount_point;
extern char *config_file;
extern char *actual_config_file;

extern struct vz_quota_stat limits;
extern struct vz_quota_ugid_stat ug_config;

extern char *command_name;
extern const char* program_name;

/* exit codes */
#define EC_SUCCESS		0
#define EC_SYSTEM		1
#define EC_USAGE		2
#define EC_VZCALL		3
#define EC_QUOTAFILE		4
#define EC_RUN			5
#define EC_NOTRUN		6
#define EC_LOCK			7
#define EC_EXDEV		8

#define EC_UG_NOTRUN		9	/* quota is running but user/group quota is inactive */
					/* this code is returned by "stat -t" command for information purposes
					 * and does not indicate a error; DO NOT USE IT! */

#define EC_QUOTADIRTY		10	/* quota is marked dirty in file */
					/* this code is returned by "show" command for information purposes
					 * and does not indicate a error; DO NOT USE IT! */

#define EC_NOQUOTAFILE		11	/* quota file does not exist */
#if 0
/*
 * Reserverd error for the future purposes
 */
#define EC_CONFIGFILE		12	/* can't open/parse VE config file */
#endif
#define EC_NOMOUNTPOINT		13	/* can't get quota mount point */

#define EC_ASSERT		255

/* log levels */
#define LOG_ERROR	0x01
#define LOG_WARNING	0x02
#define LOG_INFO	0x03
#define LOG_DEBUG	0x04

#define VE_DQ_MIN	60 /* seconds */
#define VE_DQ_HOUR	60*VE_DQ_MIN
#define VE_DQ_DAY	24*VE_DQ_HOUR
#define VE_DQ_WEEK	7*VE_DQ_DAY
#define VE_DQ_MONTH	30*VE_DQ_DAY
#define VE_DQ_YEAR	365*VE_DQ_DAY

#define FMT_BUFLEN	4*1024 /** max log output len */

#define VZQUOTA_LOG_PREFIX	"vzquota : "

extern int debug_level;
extern int batch_mode;

#define debug(level, fmt, args...) \
  (debug_level < level ? (void)0 : debug_print(level, fmt, ##args))
void error(int status, int errnum, char *oformat, ...);

#ifdef ASSERT
#undef ASSERT
#endif

#ifdef NDEBUG
#define ASSERT(condition)
#else
#define ASSERT(condition) \
    do { \
      if(!(condition)) \
        error(EC_ASSERT,0,"ASSERTION FAILED: %s(%d), function '%s()', condition: %s", \
          __FILE__, __LINE__, __FUNCTION__, #condition); \
    } while(0)
#endif

void debug_print(int level, char *oformat, ...);
void vdebug_print(int level, char *oformat, va_list pvar);

void *xmalloc(size_t size);
char *xstrdup(const char* s);

void *xrealloc(void *p, size_t size);


#define xfree(x) \
  do { \
    free(x); \
    x = NULL; \
  } while(0)

/* int str2uint(char *str, unsigned int *number); */
/* int str2u32(char *str, __u32 * number); */
/* int str2u64(char *str, __u64 * number); */

int str2time(char *str, time_t * number);
void difftime2str(time_t seconds, char *buf);
void time2str(time_t seconds, char *buf, int flags);

int str2uint(char *str, unsigned int *number);
int str2u64(char *str, __u64 * number);
int str2u32(char *str, __u32 * number);

/* Parse options type. Some commands can use the same option letters
   in various context */
#define PARSE_SETUGID	1

int parse_options(int argc, char **argv, char *short_options,
		struct option *long_options,
		char *opt_usage, int cmd_type);

/*pointers to argc and argv of main()*/
void parse_global_options(int *argc, char ***argv, const char *usg);
void usage(const char *usg);

typedef void (*func_cleaner_t) (void * data);
void register_cleaner(func_cleaner_t func, void * data);

void exit_cleanup(int status);

#endif /* __COMMON_H__ */
