#include "cmd.h"

int cmd_file(mygdb_t *mygdb, char *file) {
	if (access(file, F_OK | R_OK | X_OK)) {
		printf("Can't use file: %s\nError: %s\n", file, strerror(errno));
		return 1;
	}

	Dwarf_Error error;

	mygdb->file = malloc(strlen(file));
	strcpy(mygdb->file, file);
	
	cmdh_run_load(mygdb);

	if (dwarf_init(mygdb->fd, DW_DLC_READ, 0, 0, &mygdb->debug, &error) != DW_DLV_OK)
		return 0;

	printf("Reading symbols from %s...done.\n", file);
	return 1;
}

int cmd_run(mygdb_t *mygdb) {
	if ((mygdb->child = fork()) == -1) {
		printf("Can't fork %s\n", strerror(errno));
		return 0;
	}

	if (mygdb->child) { // PARENT
		waitpid(mygdb->child, NULL, 0);
		cmdh_bp_enable(mygdb);
		int i;
		if ((i = mygdb->bps_active) != -1) {
			printf("At breakpoint #%d (%x) on line %d\n", i, mygdb->bps[i].addr, mygdb->bps[i].line);
		}			
		ptrace(PTRACE_CONT, mygdb->child, NULL, NULL);		

	} else { // CHILD
		if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
			printf("Can't set the child to be traced.%s\n", strerror(errno));
			return 0;
		}

		char str[3] = "./";
		if (execvp(strcat(str, mygdb->file), NULL) == -1) {
			printf("Can't execute file %s\n%s\n", mygdb->file, strerror(errno));
			return 0;
		}
	}
	return 1;
}

int cmd_continue(mygdb_t *mygdb) {
	int status;
	struct user_regs_struct regs;

	if (ptrace(PTRACE_GETREGS, mygdb->child, NULL, &regs) < 0) {
		printf("Failed to get child register values.\n%s\n", strerror(errno));
		return 0;
	}

	regs.rip = regs.rip - 1;

	cmdh_get_breakpoint(mygdb);
	
	if (ptrace(PTRACE_SETREGS, mygdb->child, NULL, &regs) < 0) {
		printf("Failed to set child register values.\n%s\n", strerror(errno));
		return 0;
	}

	cmdh_restore_instruction(mygdb);

	if (ptrace(PTRACE_SINGLESTEP, mygdb->child, NULL, NULL) < 0) {
		printf("Failed at single step.\n%s\n", strerror(errno));
		return 0;	
	}

	waitpid(mygdb->child, &status, 0);	

	if (WIFEXITED(status) || WIFSIGNALED(status)) {
		mygdb->child = -1;
		mygdb->bps_enabled = -1;
		return 1;
	}

	cmdh_bp_enable(mygdb);

	if (ptrace(PTRACE_CONT, mygdb->child, NULL, NULL) < 0) {
		return 0;
	}

	return 1;
}

int cmd_break(mygdb_t *mygdb, int line_num) {
	Dwarf_Addr addr;
	if (cmdh_bp_addr(mygdb, line_num, &addr) != 1) {
		printf("Can't get address of this line %d\n", line_num);
		return 1;
	}

	if (mygdb->bps_count == -1) {
		mygdb->bps = malloc(sizeof(bp_t));
		mygdb->bps_count = 0;
	} else {
		mygdb->bps = realloc(mygdb->bps, (mygdb->bps_count + 1) * sizeof(bp_t));
		mygdb->bps_count += 1;
	}

	bp_t bp;
	bp.line = line_num;
	bp.addr = addr;

	mygdb->bps[mygdb->bps_count] = bp;

	return 1;
}

int cmd_quit() {
	return 1;
}