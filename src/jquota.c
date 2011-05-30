/*
 *  Copyright (C) 2000-2010 Parallels. All rights reserved.
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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <mntent.h>

#include "quota_io.h"
#include "common.h"
#include "linux/vzquota.h"

#define EXT4_SUPER_MAGIC        0xEF53


static struct vz_quota_stat_img *qf_data2vz_quota_stat_img(
		struct qf_data *q, struct vz_quota_stat_img *stat)
{
	stat->btime = q->stat.dq_stat.btime;
	stat->bexpire = q->stat.dq_info.bexpire;
	stat->itime = q->stat.dq_stat.itime;
	stat->iexpire = q->stat.dq_info.iexpire;

	stat->bhardlimit = q->stat.dq_stat.bhardlimit;
	stat->bsoftlimit = q->stat.dq_stat.bsoftlimit;
	stat->bcurrent = q->stat.dq_stat.bcurrent;

	stat->ihardlimit = q->stat.dq_stat.ihardlimit;
	stat->isoftlimit = q->stat.dq_stat.isoftlimit;
	stat->icurrent = q->stat.dq_stat.icurrent;
	stat->flags = q->stat.dq_info.flags;

	return stat;
}

static struct qf_data *vz_quota_stat_img2qf_data(
		struct vz_quota_stat_img *stat, struct qf_data *q)
{
	q->stat.dq_stat.btime = stat->btime;
	q->stat.dq_info.bexpire = stat->bexpire;
	q->stat.dq_stat.itime = stat->itime;
	q->stat.dq_info.iexpire = stat->iexpire;

	q->stat.dq_stat.bhardlimit = stat->bhardlimit;
	q->stat.dq_stat.bsoftlimit = stat->bsoftlimit;
	q->stat.dq_stat.bcurrent = stat->bcurrent;

	q->stat.dq_stat.ihardlimit = stat->ihardlimit;
	q->stat.dq_stat.isoftlimit = stat->isoftlimit;
	q->stat.dq_stat.icurrent = stat->icurrent;
	q->stat.dq_info.flags = stat->flags;

	return q;
}

static struct vz_quota_uginfo_img *qf_data2vz_quota_uginfo_img(
		struct qf_data *q, struct vz_quota_uginfo_img *info)
{
	bzero(info, sizeof(struct vz_quota_uginfo_img));
	info->ugid_max = q->ugid_stat.info.config.limit;
	info->pad = (q->head.flags << 0x10);
	debug(LOG_DEBUG, "\tugid_max=%d flags=%x\n", info->ugid_max, info->pad);
	return info;
}

static struct qf_data *vz_quota_uginfo_img2qf_data(
		struct vz_quota_uginfo_img *info, struct qf_data *q)
{
	q->ugid_stat.info.config.limit = info->ugid_max;
	q->head.flags = (info->pad >> 0x10);

	debug(LOG_DEBUG, "\tugid_max=%d flags=%x\n", info->ugid_max, info->pad);
	return q;
}

static struct vz_quota_ugid_stat_img *ugid_obj2vz_quota_ugid_stat_img(
		struct ugid_obj *ugid,
		struct vz_quota_ugid_stat_img *img)
{
	img->flags = VZQUOTA_UGID_PRESENT;

	img->ihardlimit = ugid->istat.qi_stat.ihardlimit;
	img->isoftlimit = ugid->istat.qi_stat.isoftlimit;
	img->icurrent = ugid->istat.qi_stat.icurrent;

	img->bhardlimit = ugid->istat.qi_stat.bhardlimit;
	img->bsoftlimit = ugid->istat.qi_stat.bsoftlimit;
	img->bcurrent = ugid->istat.qi_stat.bcurrent;

	img->btime = ugid->istat.qi_stat.btime;
	img->itime = ugid->istat.qi_stat.itime;

	return img;
}

static struct ugid_obj *vz_quota_ugid_stat_img2ugid_obj(
		int id, int type,
		struct vz_quota_ugid_stat_img *img,
		struct ugid_obj *ugid)
{
	bzero(ugid, sizeof(struct ugid_obj));

	ugid->istat.qi_id = id;
	ugid->istat.qi_type = type;

	ugid->istat.qi_stat.ihardlimit = img->ihardlimit;
	ugid->istat.qi_stat.isoftlimit = img->isoftlimit;
	ugid->istat.qi_stat.icurrent = img->icurrent;

	ugid->istat.qi_stat.bhardlimit = img->bhardlimit;
	ugid->istat.qi_stat.bsoftlimit = img->bsoftlimit;
	ugid->istat.qi_stat.bcurrent = img->bcurrent;

	ugid->istat.qi_stat.btime = img->btime;
	ugid->istat.qi_stat.itime = img->itime;

	return ugid;
}

static int write_ugid_entry(int fd, struct ugid_obj *ugid)
{
	int id, off, ret, type;
	struct vz_quota_ugid_stat_img img;

	type = ugid->istat.qi_type;
	id = ugid->istat.qi_id;
	off = VZQUOTA_UGID_OFF + (((id << 1) + type) << VZQUOTA_UGID_ITEM_BITS);

	ret = write_field(fd, ugid_obj2vz_quota_ugid_stat_img(ugid, &img),
			VZQUOTA_UGID_ITEM_SIZE, off);

	return ret;
}

int write_jquota_file(int fd, struct qf_data *q, int io_flags)
{
	int ret;

	debug(LOG_DEBUG, "Start writing journalled quota file\n");

	ret = ftruncate(fd, VZQUOTA_UGID_OFF + VZQUOTA_UGID_SIZE);
	if (ret) {
		error(0, errno, "Can't truncate");
		return 1;
	}

	/* write header */
	if (io_flags & IOF_HEAD) {
		struct vz_quota_hdr h;

		h.magic = VZQUOTA_MAGIC;
		h.version = VZQUOTA_VERSION_0;
		debug(LOG_DEBUG, "Writing header to file\n");
		ret = write_field(fd, &h, sizeof(struct vz_quota_hdr), 0);
		if (ret < 0)
			return ret;
	}

	/* write 1-level quota stat */
	if (io_flags & IOF_STAT) {
		struct vz_quota_stat_img stat;

		debug(LOG_DEBUG, "Writing 1-level quota stat to file\n");

		ret = write_field(fd, qf_data2vz_quota_stat_img(q, &stat),
				sizeof(struct vz_quota_stat_img),
				VZQUOTA_STAT_OFF);
		if (ret < 0)
			return ret;
	}
	/* write 2-level quota info */
	if (io_flags & IOF_UGID_INFO) {
		struct vz_quota_uginfo_img uinfo;

		ret = write_field(fd, qf_data2vz_quota_uginfo_img(q, &uinfo),
				sizeof(struct vz_quota_uginfo_img),
				VZQUOTA_UGINFO_OFF);
		if (ret < 0)
			return ret;
	} else if (io_flags & IOF_UGID_FLAGS) {
		int flags = (q->head.flags << 0x10);
		// update userspace flags on running quota
		// struct vz_quota_uginfo_img->pad
		debug(LOG_DEBUG, "update uguid flags=%X\n", q->head.flags);
		ret = write_field(fd, &flags,
				sizeof(__le32),
				VZQUOTA_UGINFO_OFF + sizeof(__le32));
		if (ret < 0)
			return ret;
	}

	/* write ugid objects */
	if (io_flags & IOF_UGID_BUF) {
		ret = ftruncate(fd, VZQUOTA_UGID_OFF);
		ret = ftruncate(fd, VZQUOTA_UGID_OFF + VZQUOTA_UGID_SIZE);
		if (ret) {
			error(0, errno, "Can't truncate");
			return 1;
		}

		debug(LOG_DEBUG, "Writing ugid objects to file: %u entries total\n",
				q->ugid_stat.dquot_size);

		if (q->ugid_stat.info.buf_size != q->ugid_stat.dquot_size)
			error(EC_QUOTAFILE, 0, "Number of stored ugids objects is not equal to "
					"one in 2-level quota statistics; "
					"quota file is corrupted for id %d [%d!=%d]",
					quota_id, q->ugid_stat.info.buf_size,  q->ugid_stat.dquot_size);

		if (q->ugid_stat.dquot_size > 0) {
			int err;
			size_t i;
			struct dquot *dq;

			reset_dquot_search();
			for (i = 0; ; i++) {
				dq = get_next_dquot(&(q->ugid_stat));
				if (dq == NODQUOT)
					break;
				err = write_ugid_entry(fd, &dq->obj);
				if (err < 0)
					return err;
			}
		}
	}

	fsync(fd);
	fdatasync(fd);

	return 0;
}

static int read_quota_file_2l(int fd, struct qf_data *q)
{
	int ret, nr_items, i;
	struct vz_quota_uginfo_img uginf;
	struct vz_quota_ugid_stat_img ugstat;

	debug(LOG_DEBUG, "Reading ugid objects from file\n");
	if (sizeof(struct vz_quota_ugid_stat_img) > VZQUOTA_UGID_ITEM_SIZE)
		error(EC_QUOTAFILE, 0, "Sizes error %d vs %d",
				sizeof(struct vz_quota_ugid_stat_img), VZQUOTA_UGID_ITEM_SIZE);

	if (lseek(fd, VZQUOTA_UGINFO_OFF, SEEK_SET) != VZQUOTA_UGINFO_OFF)
		error(EC_QUOTAFILE, errno, "Can't rewind file");

	ret = read(fd, &uginf, sizeof(uginf));
	if (ret != sizeof(struct vz_quota_uginfo_img))
		error(EC_QUOTAFILE, errno, "Can't read ugidinfo");

	debug(LOG_DEBUG, "Graces: ugid_max=%d u %llu:%llu g %llu:%llu\n",
			uginf.ugid_max,
			(unsigned long long)uginf.uid_iexpire,
			(unsigned long long)uginf.uid_bexpire,
			(unsigned long long)uginf.gid_iexpire,
			(unsigned long long)uginf.gid_bexpire);

	if (lseek(fd, VZQUOTA_UGID_OFF, SEEK_SET) != VZQUOTA_UGID_OFF)
		error(EC_QUOTAFILE, errno, "Can't rewind file");

	nr_items = 1 << (VZQUOTA_UGID_BITS - VZQUOTA_UGID_ITEM_BITS);
	debug(LOG_DEBUG, "UGID entries (%d max):\n", nr_items);
	free_ugid_quota(&(q->ugid_stat));
	for (i = 0; i < nr_items; i++) {
		struct ugid_obj ugid;

		ret = read(fd, &ugstat, VZQUOTA_UGID_ITEM_SIZE);
		if (ret != VZQUOTA_UGID_ITEM_SIZE)
			error(EC_QUOTAFILE, errno, "Can't read ugidinfo");

		if (!(ugstat.flags & VZQUOTA_UGID_PRESENT))
			continue;

		debug(LOG_DEBUG, "%c %d:\n", i & 1 ? 'g' : 'u', i >> 1);
		debug(LOG_DEBUG, "\tBlocks: %Lu <%Lu:%Lu>\n",
				(unsigned long long)ugstat.bcurrent,
				(unsigned long long)ugstat.bsoftlimit,
				(unsigned long long)ugstat.bhardlimit);
		debug(LOG_DEBUG, "\tInodes: %Lu <%Lu:%Lu>\n",
				(unsigned long long)ugstat.icurrent,
				(unsigned long long)ugstat.isoftlimit,
				(unsigned long long)ugstat.ihardlimit);
		add_dquot(&(q->ugid_stat), vz_quota_ugid_stat_img2ugid_obj(
						(i >> 1), (i & 1), &ugstat, &ugid));
		q->ugid_stat.info.buf_size++;
	}

	return 0;
}

int read_jquota_file(int fd, struct qf_data *q, int io_flags)
{
	off_t err;

	debug(LOG_DEBUG, "Start reading journalled quota file\n");
	/* read header */
	if (io_flags & IOF_HEAD) {
		struct vz_quota_hdr h;

		debug(LOG_DEBUG, "Reading header from file\n");
		err = read_field(fd, &h, sizeof(struct vz_quota_hdr), 0);
		if (err < 0)
			return err;
		q->head.magic = h.magic;
		q->version = h.version;
		if (q->version != VZQUOTA_VERSION_0) {
			debug(LOG_ERROR, "Unsupported quota version %d\n", q->version);
			return -1;
		}
	}
	/* read 1-level quota stat */
	if (io_flags & IOF_STAT) {
		struct vz_quota_stat_img stat;

		debug(LOG_DEBUG, "Reading 1-level quota stat from file\n");
		err = read_field(fd, &stat, sizeof(struct vz_quota_stat_img),
				VZQUOTA_STAT_OFF);
		if (err < 0)
			return err;
		vz_quota_stat_img2qf_data(&stat, q);
	}
	/* read 2-level quota info */
	if (io_flags & IOF_UGID_INFO) {
		struct vz_quota_uginfo_img uinfo;

		debug(LOG_DEBUG, "Reading 2-level quota info from file\n");
		err = read_field(fd, &uinfo, sizeof(struct vz_quota_stat_img),
				VZQUOTA_UGINFO_OFF);
		if (err < 0)
			return err;

		debug(LOG_DEBUG, "");
		vz_quota_uginfo_img2qf_data(&uinfo, q);
	}
	 if (io_flags & IOF_UGID_BUF)
		read_quota_file_2l(fd, q);

	return 0;
}

int do_jquota_on_ioctl(unsigned int id, struct vz_quota_stat *qstat, char *dir)
{
	int fd, ret;
	struct vzctl_quotactl ctl;

	fd = open(VZCTL_DEVICE, O_RDONLY);
	if (fd < 0) {
		error(0, errno, "Can't open %s", VZCTL_DEVICE);
		return -1;
	}

	debug(LOG_DEBUG, "VZ_DQ_ON_FILE %s\n", dir);
	ctl.cmd = VZ_DQ_ON_FILE;
	ctl.quota_id = id;
	ctl.qstat = qstat;
	ctl.ve_root = dir;

	ret = ioctl(fd, VZCTL_QUOTA_NEW_CTL, &ctl);
	if (ret) {
		if (errno == EINVAL)
			error(0, 0, "Kernel does not support journalled quota.");
		else
			error(0, errno, "VZCTL_QUOTA_NEW_CTL: filed to turn quota on");
	}
	close(fd);

	return ret;
}

int do_jquota_off_ioctl(unsigned int id)
{
	int fd, ret;
	struct vzctl_quotactl ctl;

	fd = open("/dev/vzctl", O_RDONLY);
	if (fd < 0) {
		error(0, errno, "Can't open /dev/vzctl");
		return -1;
	}

	debug(LOG_DEBUG, "VZ_DQ_OFF_FILE\n");
	ctl.cmd = VZ_DQ_OFF_FILE;
	ctl.quota_id = id;
	ctl.qstat = NULL;
	ctl.ve_root = NULL;

	ret = ioctl(fd, VZCTL_QUOTA_NEW_CTL, &ctl);
	if (ret) {
		if (errno == ENOENT) {
			error(0, errno, "Quota is not running for id %d", id);
			ret = 0;
		} else if (errno == EBUSY) {
			error(0, errno, "Quota off for id %d", quota_id);
			error(0, 0, "\tPossible reasons:");
			error(0, 0, "\t- Container's root is not unmounted");
			error(0, 0, "\t- there are opened files inside Container's root/prvate"
					" area");
			error(0, 0, "\t- your current working directory is inside Container's");
			error(0, 0, "\t  root/private area");
			exit(EC_VZCALL);
		} else {
			error(EC_VZCALL, errno, "Quota off failed");
		}
	}
	close(fd);

	return ret;
}

int do_jquota_status_ioctl(unsigned int id)
{
	int fd, ret;
	struct vzctl_quotactl ctl;

	fd = open("/dev/vzctl", O_RDONLY);
	if (fd < 0) {
		error(0, errno, "Can't open /dev/vzctl");
		return -1;
	}

	ctl.cmd = VZ_DQ_STATUS;
	ctl.quota_id = id;
	ctl.qstat = NULL;
	ctl.ve_root = NULL;

	ret = ioctl(fd, VZCTL_QUOTA_NEW_CTL, &ctl);
	if (ret == -1) {
		if (errno == ESRCH)
			ret = 0;
		else if (errno != EINVAL)
			error(0, errno, "VZ_DQ_STATUS: filed to get jquota status");
	}
	close(fd);
	debug(LOG_DEBUG, "jquota_status=%d\n", ret);
	return ret;
}

int is_jquota_mode(int fd)
{
	struct vz_quota_hdr h;

	if (read_field(fd, &h, sizeof(struct vz_quota_hdr), 0) < 0)
		error(EC_SYSTEM, errno, "read error");

	if (h.magic == VZQUOTA_MAGIC)
		return 1;

	return 0;
}

int check_jquota_file(int fd)
{
	if (!is_jquota_mode(fd)) {
                debug(LOG_WARNING, "Quota magic is invalid for id %d\n", quota_id);
		return -1;
	}
        return 0;
}

void validate_fstab(char *path)
{
	struct mntent *mnt;
	FILE *fp;
	struct stat st1, st2;

	if (stat(path, &st1) != 0) {
		error(0, errno, "failed to stat %s", path);
		return;
	}

	if ((fp = setmntent("/etc/fstab", "r")) == NULL) {
		error(0 , errno, "Failed to upen /etc/fstab");
		return;
	}
	while ((mnt = getmntent(fp)) != NULL) {
		if (stat(mnt->mnt_dir, &st2) != 0)
			continue;
		if (st1.st_dev != st2.st_dev)
			continue;
		if (mnt->mnt_passno != 0)
			error(EC_FSCK_AUTOCHECK, 0, "File system %s is marked for fsck check in the /etc/fstab.\n"
					"journalled quota support is not possible!",
					mnt->mnt_dir);
	}
	endmntent(fp);
}

int is_jquota_supported(char *path)
{
	struct mntent *mnt;
	FILE *fp;
	int ret = 0;
	struct stat st1, st2;

	if (stat(path, &st1)) {
		return 1;
	}
	if ((fp = setmntent("/proc/mounts", "r")) == NULL) {
		error(0, errno, "Failed to upen /proc/mounts");
		return 0;
	}
	while ((mnt = getmntent(fp)) != NULL) {
		if (stat(mnt->mnt_dir, &st2) != 0)
			continue;
		if (st1.st_dev != st2.st_dev)
			continue;
		if (mnt->mnt_type != NULL &&
		    strcmp(mnt->mnt_type, "ext4") == 0)
		{
			ret = 1;
			break;
		}
	}
	endmntent(fp);
	return ret;
}

int is_jquota_disabled()
{
	return 0;
}
