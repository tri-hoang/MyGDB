#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmd.h"

// Maximum number of arguments
#define ARGS_NUM 	2
// Split between arguments
#define ARGS_DELIM 	" \n"

char *mygdb_read_line();
char **mygdb_parse_args(char *, int *);
int mygdb_execute(mygdb_t *, char **, int);
void mygdb_init(mygdb_t *);

int main(int argc, char const *argv[])
{
	char *line;
	char **args;
	int args_count, i;
	cmd_t status = CMD_GO;
	mygdb_t mygdb;
	mygdb_init(&mygdb);

	do {
		printf("(mygdb) ");

		line = mygdb_read_line();
		args = mygdb_parse_args(line, &args_count);
		status = mygdb_execute(&mygdb, args, args_count);

		int stat;
		if (status == CMD_QUIT) {
			return 0;
		}
		if (mygdb.child != -1 && (status == CMD_RUN || status == CMD_CONTINUE)) {
			waitpid(mygdb.child, &stat, 0);
			if (WIFEXITED(stat) || WIFSIGNALED(stat)) {
				mygdb.child = -1;
				mygdb.bps_enabled = -1;
				printf("Program ended.\n");
				continue;
			}

			cmdh_get_breakpoint(&mygdb);

			if (mygdb.bps_active != -1) {
				int ii = mygdb.bps_active;
				printf("Stopped breakpoint #%d (%x) on line %d\n", ii, mygdb.bps[ii].addr, mygdb.bps[ii].line);
			}
		}
		free(line);
		free(args);
	} while (status);
	return 0;
}

char *mygdb_read_line() {
	char *line 	= NULL;
	size_t buff = 0;
	getline(&line, &buff, stdin);
	return line;
}

char **mygdb_parse_args(char *line, int *args_count) {
	int pos = 0;
	char **tokens = malloc(ARGS_NUM * sizeof(char *));
	char *token;

	if (!tokens) {
		fprintf(stderr, "Can't allocate memory in mygdb_parse_args()\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(line, ARGS_DELIM);
	while (token != NULL && pos < ARGS_NUM) {
		tokens[pos++] = token;
		token = strtok(NULL, ARGS_DELIM);
	}
	tokens[pos] = NULL;
	*args_count = pos;
	return tokens;
}

int mygdb_execute_check(mygdb_t *);
// Read in arguments and execute command appropriately
int mygdb_execute(mygdb_t *mygdb, char **args, int args_count) {
	if (args_count == 0) return 1;

	if (!strcmp(args[0], EXEC_FILE)) {
		if (args_count != 2) 
			return 1;
		return cmd_file(mygdb, args[1]);

	} else if (!strcmp(args[0], EXEC_RUN)) {
		if (mygdb_execute_check(mygdb)) return 1;

		return cmd_run(mygdb);

	} else if (!strcmp(args[0], EXEC_CONTINUE)) {
		if (mygdb_execute_check(mygdb)) return 1;
		if (mygdb->child == -1) {
			printf("The program is not running.\n");
			return 1;
		}
		return cmd_continue(mygdb);

	} else if (!strcmp(args[0], EXEC_BREAK)) {
		if (mygdb_execute_check(mygdb)) return 1;
		if (args_count != 2) 
			return 1;
		return cmd_break(mygdb, atoi(args[1]));

	} else if (!strcmp(args[0], EXEC_QUIT)) {
		// if (mygdb_execute_check(mygdb)) return 1;
		return cmd_quit(mygdb);
	} else if (!strcmp(args[0], EXEC_PRINT)) {
		if (mygdb_execute_check(mygdb)) return 1;
		if (args_count != 2) return 1;
		return cmd_print(mygdb, args[1]);
	} else {
		printf("Command unknown\n");
		return 1;
	}
}

// Return 1 if debug has not been set
int mygdb_execute_check(mygdb_t *mygdb) {
	if (mygdb->debug == NULL) {
		printf("Please specify the file to use first with \"file exec_file\".\n");
		return 1;
	} else return 0;
} 

void mygdb_init(mygdb_t *mygdb) {
	mygdb->bps_enabled = -1;
	mygdb->bps_count = -1;
	mygdb->child = -1;
	mygdb->bps_active = -1;
	return;
}