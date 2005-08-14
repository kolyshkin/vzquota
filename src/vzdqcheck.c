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
#ifdef L2
	info.ugid_stat = NULL;
#endif	
	scan(&info, mount_point);

	printf("quota usage for %s\n", mount_point);
	printf("%11s%11s\n","blocks", "inodes");
#ifndef L2
	printf("%11u%11u\n", block_view(info.size), info.inodes);
#else
	printf("%11u%11u\n", ker2block(info.size), info.inodes);
#endif
	exit(0);
}

