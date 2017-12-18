#include "cmd.h"

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

cmd_t cmd_quit() {
	return CMD_QUIT;
}

cmd_t cmd_print(mygdb_t *mygdb, char *var) {
	Dwarf_Half tag;
	Dwarf_Error err;
	Dwarf_Die die;
	Dwarf_Die die_child;

	if (cmdh_var_func(mygdb, &die) == RE_FATAL) {
		printf("Couldn't execute cmdh_var_func() in cmd.c\n");
		return CMD_END;
	}

	printf("CMD_PRINT\n");
	if (dwarf_tag(die, &tag, &err) != DW_DLV_OK) {
		printf("dwarf_tag() error in cmd.c\n%s\n", dwarf_errmsg(err));
		return CMD_END;
	}

	if (tag != DW_TAG_subprogram) {
		printf("Only need to view subprogram DIE.\n");
		return CMD_GO;
	}

	if (dwarf_child(die, &die_child, &err) == DW_DLV_ERROR) {
		printf("dwarf_child() error in cmd.c\n%s\n", dwarf_errmsg(err));
		return CMD_END;
	}


	while (1) {
		int rc, found = 0;

		if (cmdh_var_check(die_child, var, &found) == RE_FATAL) {
			printf("cmdh_var_check() error in cmd.c\n");
			return CMD_END;
		}

		if (found) {
			return cmdh_var_print(mygdb, die_child, var);
		}

		if ((rc = dwarf_siblingof(mygdb->t_dbg, die_child, &die_child, &err)) == DW_DLV_ERROR) {
			printf("Error at dwarf_siblingof() in cmd.c\n%s\n", dwarf_errmsg(err));
			return CMD_END;
		} else if (rc == DW_DLV_NO_ENTRY) {
			printf("Couldn't find the variable.\n");
			return CMD_GO;
		}

	}
	return CMD_END;
}