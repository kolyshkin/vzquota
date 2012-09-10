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
	    .cmd	= _cmd,
	    .quota_id	= _quota_id,
	    .qstat	= _qstat,
	    .ve_root	= (char *) _ve_root
	};

//	ASSERT(_qstat);

	debug(LOG_DEBUG, "vzquotactl ioctl start:cmd %d: id %d\n",
		_cmd, _quota_id);

	fd = open(VZCTL_DEVICE, O_RDWR);
	if (fd < 0)
		error(EC_VZCALL, errno, "can't open vzctl device '%s'", VZCTL_DEVICE);

	debug(LOG_DEBUG, "attempt new ioctl[%d]\n", VZCTL_QUOTA_CTL);
	rc = ioctl(fd, VZCTL_QUOTA_NEW_CTL, &qu);
	debug(LOG_DEBUG, "vzquotactl ioctl end:cmd %d: id %d: status %d\n",
		_cmd, _quota_id, rc);

	close(fd);
	return rc;
}

long vzquotactl_ugid_syscall(
		int _cmd,		 /* subcommand */
		unsigned int _quota_id,  /* quota id where it applies to */
		unsigned int _ugid_index,/* for reading statistic. index of first
					    uid/gid record to read */
		unsigned int _ugid_size, /* size of ugid_buf array */
		void *_addr		 /* user-level buffer */
		)
{
	int rc;
	int fd;
	struct vzctl_quotaugidctl qu =
	{
		.cmd		= _cmd,
		.quota_id	= _quota_id,
		.ugid_index	= _ugid_index,
		.ugid_size	= _ugid_size,
		.addr		= _addr
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
