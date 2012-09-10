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

#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "vzquota.h"
#include "quotacheck.h"

const char *program_name = "vzdqcheck";
char *command_name = NULL; /* for usage() */

char* globolize_path(char *path);

static char usg[] =
"Usage: %s [options] path\n"
"\t-h\thelp\n"
"\t-V\tversion info\n"
"\tpath\tscan path\n";

int main(int argc, char **argv)
{
	struct scan_info info;

	parse_global_options(&argc, &argv, usg);

	mount_point = globolize_path(argv[0]);
	info.ugid_stat = NULL;
	scan(&info, mount_point);

	printf("quota usage for %s\n", mount_point);
	printf("%11s%11s\n","blocks", "inodes");
	printf("%11llu%11u\n", ker2block(info.size), info.inodes);
	exit(0);
}

