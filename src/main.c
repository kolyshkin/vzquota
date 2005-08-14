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

#include "vzquota.h"
#include "common.h"

char *command_name;
const char *program_name = "vzquota";

static char usg[] = 
"Usage: %s [options] command quotaid [command-options-and-arguments]\n";

static char cmd_usage[] =
"vzquota commands are:\n"
"\tinit       Initialize quota data for given quotaid\n"
"\ton         Turn on quota accounting for given quotaid\n"
"\toff        Turn off quota accounting for given quotaid\n"
"\tdrop       Delete quota limits from file\n"
"\tsetlimit   Set quota limits for given quotaid\n"
"\tsetlimit2  Set L2 quota limits for given quotaid and QUGID\n"
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
	"stat", vestat_proc}, {
	"show", quotashow_proc}, {
	NULL, NULL}
};

int main(int argc, char **argv)
{
	int err;
	const struct cmd *cm;

	parse_global_options(&argc, &argv, usg);

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

