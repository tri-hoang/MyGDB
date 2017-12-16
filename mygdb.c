#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARGS_NUM 	2
#define ARGS_DELIM 	" \n"

char *mygdb_read_line();
char **mygdb_parse_args(char *, int *);
int mygdb_execute(char **);

int main(int argc, char const *argv[])
{
	char *line;
	char **args;
	int status = 1, args_count, i;
	do {
		printf("(mygdb) ");

		line = mygdb_read_line();
		args = mygdb_parse_args(line, &args_count);
		status = mygdb_execute(args);

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

int mygdb_execute(char **args) {
	return 1;
}