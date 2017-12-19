#include "cmd.h"

// Load file DWARF values
cmd_t cmd_file(mygdb_t *mygdb, char *file) {
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
	return CMD_FILE;
}

// Kill the child process if one is alreayd running
// Otherwise set it up and enable breakpoints
cmd_t cmd_run(mygdb_t *mygdb) {
	// If "run" is called when a child is running, kill the child and reset control values.
	if (mygdb->child != -1) {
		kill(mygdb->child, SIGTERM);
		mygdb->child = -1;
		mygdb->bps_active = -1;
		mygdb->bps_enabled = -1;
	}

	if ((mygdb->child = fork()) == -1) {
		printf("Can't fork %s\n", strerror(errno));
		return 0;
	}

	if (mygdb->child) { // PARENT
		waitpid(mygdb->child, NULL, 0);
		cmdh_bp_enable(mygdb);		
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
	return CMD_RUN;
}

// Reload ptrace and waitpid again
cmd_t cmd_continue(mygdb_t *mygdb) {
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

	return CMD_CONTINUE;
}

// Set breakpoints
cmd_t cmd_break(mygdb_t *mygdb, int line_num) {
	Dwarf_Addr addr;
	if (cmdh_bp_addr(mygdb, line_num, &addr) != 1) {
		printf("Can't get address of this line %d\n", line_num);
		return 1;
	}

	if (mygdb->bps_count == -1) {
		mygdb->bps_count = 1;
		mygdb->bps = malloc(sizeof(bp_t));
	} else {
		mygdb->bps_count += 1;
		mygdb->bps = realloc(mygdb->bps, (mygdb->bps_count) * sizeof(bp_t));
	}

	bp_t bp;
	bp.line = line_num;
	bp.addr = addr;

	mygdb->bps[mygdb->bps_count - 1] = bp;

	return CMD_BREAK;
}

// Quit the program
cmd_t cmd_quit(mygdb_t *mygdb) {
	Dwarf_Error err;
	if (mygdb->child != -1) kill(mygdb->child, SIGKILL);
	if (mygdb->debug != NULL && (dwarf_finish(mygdb->debug, &err) == DW_DLV_ERROR)) {
		printf("Couldn't quit\n%s\n", dwarf_errmsg(err));
		return CMD_GO;
	}
	return CMD_QUIT;
}

// Print the variable 
cmd_t cmd_print(mygdb_t *mygdb, char *var) {
	Dwarf_Die fun_die;
	Dwarf_Die var_die;
	struct user_regs_struct regs;

	if (ptrace(PTRACE_GETREGS, mygdb->child, NULL, &regs) < 0) {
		printf("Error at ptrace getregs in cmd.c@cmd_print\n%s\n", strerror(errno));
		return CMD_END;
	}

	if (cmdh_print_getFunc(mygdb, &fun_die, &regs) == RE_FATAL) {
		printf("Can't execute cmdh_print_getFunc in cmd.c@cmd_print\n");
		return CMD_END;
	}

	if (cmdh_print_getVar(mygdb, fun_die, &var_die, var) == RE_FATAL) {
		printf("Can't execute cmdh_print_getVar in cmd.c@cmd_print_\n");
		return CMD_END;
	}

	if (cmdh_print_printVar(mygdb, var_die, var) == RE_FATAL) {
		printf("Can't execute cmdh_print_varPrint in cmd.c@cmd_print\n");
		return CMD_END;
	}

	return CMD_GO;

}