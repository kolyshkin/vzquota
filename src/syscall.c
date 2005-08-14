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

#include <asm/unistd.h>
#include <sys/file.h>

#include "vzquota.h"
#include "common.h"

#ifdef VZCTL_QUOTA_CTL

#define VZCTL_DEVICE "/dev/vzctl"

#include <sys/ioctl.h>

long vzquotactl_syscall(
		int _cmd,
		unsigned int _quota_id,
		struct vz_quota_stat *_qstat,
		const char *_ve_root)
{
	int rc;
	int fd;
	struct vzctl_quotactl qu =
	{
	    cmd		: _cmd,
	    quota_id	: _quota_id,
	    qstat	: _qstat,
	    ve_root	: (char *) _ve_root  
	};

//	ASSERT(_qstat);
	
	debug(LOG_DEBUG, "vzquotactl ioctl start:cmd %d: id %d\n",
		_cmd, _quota_id);
	
	fd = open(VZCTL_DEVICE, O_RDWR);
	if (fd < 0)
		error(EC_VZCALL, errno, "can't open vzctl device '%s'", VZCTL_DEVICE);

	debug(LOG_DEBUG, "attempt new ioctl[%d]\n", VZCTL_QUOTA_CTL);
#ifndef L2
	rc = ioctl(fd, VZCTL_QUOTA_CTL, &qu);
	if (rc < 0 && (errno == ENOTTY || errno == EINVAL))
	{
		/* attempt old quota syscall */
		struct vz_quota_stat_old qstat_old = quota_new2old(_qstat);
		debug(LOG_DEBUG, "attempt old ioctl [%d]\n", VZCTL_QUOTA_CTL_OLD);

		qu.qstat = (struct vz_quota_stat *) &qstat_old;
		rc = ioctl(fd, VZCTL_QUOTA_CTL_OLD, &qu);
		*_qstat = quota_old2new(&qstat_old);
	}
#else
	rc = ioctl(fd, VZCTL_QUOTA_NEW_CTL, &qu);
#endif
	debug(LOG_DEBUG, "vzquotactl ioctl end:cmd %d: id %d: status %d\n",
		_cmd, _quota_id, rc);
	
	close(fd);	
	return rc;
}

#ifdef L2
long vzquotactl_ugid_syscall(
		int _cmd,                /* subcommand */
		unsigned int _quota_id,  /* quota id where it applies to */
		unsigned int _ugid_index,/* for reading statistic. index of first
					    uid/gid record to read */
		unsigned int _ugid_size, /* size of ugid_buf array */
		void *_addr               /* user-level buffer */
		)
{
	int rc;
	int fd;
	struct vzctl_quotaugidctl qu =
	{
		cmd		: _cmd,
		quota_id	: _quota_id,
		ugid_index	: _ugid_index,
		ugid_size	: _ugid_size,
		addr		: _addr
	};

	debug(LOG_DEBUG, "vzquotaugidctl ioctl start:cmd %d: id %d\n",
		_cmd, _quota_id);
	
	fd = open(VZCTL_DEVICE, O_RDWR);
	if (fd < 0)
		error(EC_VZCALL, errno, "can't open vzctl device '%s'", VZCTL_DEVICE);

	rc = ioctl(fd, VZCTL_QUOTA_UGID_CTL, &qu);

	debug(LOG_DEBUG, "vzquotaugidctl ioctl end:cmd %d: id %d: status %d\n",
		_cmd, _quota_id, rc);
	
	close(fd);	
	return rc;
}
#endif

#else

vza
_syscall4(long, vzquotactl, int, cmd, unsigned int, quota_id,
	  struct vz_quota_stat *, qstat, const char *, ve_root);

long vzquotactl_syscall(int cmd, unsigned int quota_id,
			struct vz_quota_stat *qstat, const char *ve_root)
{
	long status;
	debug(LOG_DEBUG, "vzquotactl call start:cmd %d: id %d\n",
		cmd, quota_id);
	status = vzquotactl(cmd, quota_id, qstat, ve_root);
	debug(LOG_DEBUG, "vzquotactl call end:cmd %d: id %d: status %d\n",
		cmd, quota_id, status);
	return status;
}

#endif
