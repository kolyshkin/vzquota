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

#include "vzquota.h"
#include "common.h"

char *command_name;
const char *program_name = "vzquota";

static char cmd_usage[] =
"Usage: %s [options] command quotaid [command-options-and-arguments]\n\n"
"vzquota commands are:\n"
"\tinit       Initialize quota data for given quotaid\n"
"\ton         Turn on quota accounting for given quotaid\n"
"\toff        Turn off quota accounting for given quotaid\n"
"\tdrop       Delete quota limits from file\n"
"\tsetlimit   Set quota limits for given quotaid\n"
"\tsetlimit2  Set L2 quota limits for given quotaid and QUGID\n"
"\treload2    Reload L2 quota limits from quota file for given quotaid\n"
"\tstat       Show usage and quota limits for given quotaid\n"
"\tshow       Show usage and quota limits from quota file\n";

static const struct cmd {
	char *fullname;		/* Full name of the function (e.g. "commit") */
	int (*func) ();		/* Function takes (argc, argv) arguments. */
} cmds[] = {
	{
	"on", quotaon_proc}, {
	"off", quotaoff_proc}, {
	"init", quotainit_proc}, {
	"drop", quotadrop_proc}, {
	"setlimit", quotaset_proc}, {
	"setlimit2", quotaugidset_proc}, {
	"reload2", quotareloadugid_proc}, {
	"stat", vestat_proc}, {
	"show", quotashow_proc}, {
	NULL, NULL}
};

int main(int argc, char **argv)
{
	int err;
	const struct cmd *cm;

	parse_global_options(&argc, &argv, cmd_usage);

	/* Look up the command name. */
	command_name = argv[0];
	for (cm = cmds; cm->fullname; cm++) {
		if (!strcmp(command_name, cm->fullname))
			break;
	}
	if (!cm->fullname) {
		fprintf(stderr, "Unknown command: '%s'\n\n", command_name);
		usage(cmd_usage);
	} else
		command_name = cm->fullname;

	err = (*(cm->func)) (argc, argv);
	exit(err);
}

