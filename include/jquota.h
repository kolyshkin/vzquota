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
 */ 
#ifndef __JQUOTA_H__
#define __JQUOTA_H__

int write_jquota_file(int fd, struct qf_data *q, int io_flags);
int read_jquota_file(int fd, struct qf_data *q, int io_flags);

int do_jquota_on_ioctl(unsigned int id, struct vz_quota_stat *stat, char *dir);
int do_jquota_off_ioctl(unsigned int id);
int do_jquota_status_ioctl(unsigned int id);

int is_jquota_mode(int fd);
int check_jquota_file(int fd);
void validate_fstab(char *path);
int is_jquota_supported(char *path);
int is_jquota_disabled();

#endif
