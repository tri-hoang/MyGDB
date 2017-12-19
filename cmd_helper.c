#include "cmd_helper.h"

// Load DWARF data
int cmdh_run_load(mygdb_t *mygdb) {
	Dwarf_Die no_die = 0;
	Dwarf_Unsigned cu_header_length;
	Dwarf_Unsigned abbrev_offset;
	Dwarf_Unsigned next_cu_header;
	Dwarf_Half version_stamp;
	Dwarf_Half address_size;
	Dwarf_Error err;

	mygdb->fd = open(mygdb->file, O_RDONLY);

	if (dwarf_init(mygdb->fd, DW_DLC_READ, 0, 0, &mygdb->debug, &err) != DW_DLV_OK) {
		printf("Failed at dwarf_init() in cmd_helper.c\n%s\n", dwarf_errmsg(err));
		return 0;
	}

	if (dwarf_next_cu_header(mygdb->debug, 
							&cu_header_length, 
							&version_stamp, 
							&abbrev_offset, 
							&address_size, 
							&next_cu_header, &err) == DW_DLV_ERROR) {
		printf("Failed at dwarf_next_cu_header() in cmd_helper.c\n%s\n", dwarf_errmsg(err));
		return 0;
	}

	if (dwarf_siblingof(mygdb->debug, no_die, &mygdb->cu_die, &err) == DW_DLV_ERROR) {
		printf("Failed at dwarf_siblingof() in cmd_helper.c\n%s\n", dwarf_errmsg(err));
		return 0;
	}

	mygdb->t_dbg = mygdb->debug;
	return 1;
}

// Get the address of line number
int cmdh_bp_addr(mygdb_t *mygdb, int line_num, Dwarf_Addr *addr) {
	Dwarf_Line *linebuf;
	Dwarf_Unsigned l_num;
	Dwarf_Signed count;
	Dwarf_Error error;
	int i = 0;

	if (dwarf_srclines(mygdb->cu_die, &linebuf, &count, &error) == DW_DLV_ERROR) {
		printf("Failed at dwarf_srclines() in cmd_helper.c\n%s\n", dwarf_errmsg(error));
		return 0;
	}

	for (i = 0; i < count; i++) {

		if (dwarf_lineno(linebuf[i], &l_num, &error) == DW_DLV_ERROR) {
			printf("Failed at dwarf_lineno() in cmd_helper.c\n%s\n", dwarf_errmsg(error));
			return 0;
		}
		if (l_num == line_num) {
			if (dwarf_lineaddr(linebuf[i], addr, &error) == DW_DLV_ERROR) {
				printf("Failed at dwarf_lineaddr() in cmd_helper.c\n%s\n", dwarf_errmsg(error));
			} else break;
		}
		dwarf_dealloc(mygdb->debug, linebuf[i], DW_DLA_LINE);
	}
	dwarf_dealloc(mygdb->debug, linebuf, DW_DLA_LIST);
	printf("Line %d: 0x%d\n", line_num, *addr);
	return 1;
}

// Enable all breakpoints
int cmdh_bp_enable(mygdb_t *mygdb) {
	if (mygdb->bps_enabled == mygdb->bps_count) return CMD_GO;
	if (mygdb->bps_enabled == -1) mygdb->bps_enabled = 0;
	for (; mygdb->bps_enabled < mygdb->bps_count; mygdb->bps_enabled++) {
		int i = mygdb->bps_enabled;
		mygdb->bps[i].original = ptrace(PTRACE_PEEKTEXT, mygdb->child, (void *) mygdb->bps[i].addr, 0);
		if (errno) {
			printf("Can't read original.\n%s\n", strerror(errno));
			return 0;
		}
		if (ptrace(PTRACE_POKETEXT, mygdb->child, (void *) mygdb->bps[i].addr, (mygdb->bps[i].original & HIGH_BIT_MASK) | 0xCC) < 0) {
			printf("Can't set breakpoint.\n%s\n", strerror(errno));
			return 0;
		};
	}

	return 1;
}

// Restore instructions
int cmdh_restore_instruction(mygdb_t *mygdb) {
	long data;
	bp_t bp = mygdb->bps[mygdb->bps_active];
	// printf("%d\n", bp.addr);

	data = ptrace(PTRACE_PEEKTEXT, mygdb->child, (void*) bp.addr, 0);
	if (errno) {
		printf("Can't peek text in cmd_helper.c@cmdh_restore_instruction()\n%s\n", strerror(errno));
		return 0;
	};

	if (ptrace(PTRACE_POKETEXT, mygdb->child, (void *) bp.addr, (data & HIGH_BIT_MASK) | (bp.original & 0xFF)) < 0) {
		printf("Can't poke text in cmd_helper.c@cmdh_restore_instruction()\n%s\n", strerror(errno));
		return 0;
	}
	return 1;
}

// Get the current active breakpoint
int cmdh_get_breakpoint(mygdb_t *mygdb) {
	int i;
	struct user_regs_struct regs;

	if (ptrace(PTRACE_GETREGS, mygdb->child, NULL, &regs) < 0) {
		printf("Can't get regs in cmd_helper.c@cmdh_get_breakpoint()\n%s\n", strerror(errno));
	}

	mygdb->bps_active = -1;

	for (i = 0; i < mygdb->bps_enabled; i++) {
		if ((regs.rip - 1) == mygdb->bps[i].addr) {
			mygdb->bps_active = i;
			break;
		}
	}
}

// Look through DIEs and check for a right DIE
int cmdh_print_getFunc(mygdb_t * mygdb, Dwarf_Die *die, struct user_regs_struct *regs) {
	int rc;
	Dwarf_Die child_die;
	Dwarf_Error err;

	if (dwarf_child(mygdb->cu_die, &child_die, &err) == DW_DLV_ERROR) {
		printf("Error at dwarf_child in cmd_helper.c@cmdh_print_getFunc\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	while (1) {
		int found = 0;
		if (cmdh_print_getFunc_findFunc(child_die, regs, &found) == RE_FATAL) {
			printf("Can't execute cmdh_print_getFunc_findFunc in cmd_helper.c@cmdh_print_getFunc\n");
			return RE_FATAL;
		}

		if (found) {
			*die = child_die;
			return RE_OK;
		}

		if ((rc = dwarf_siblingof(mygdb->t_dbg, child_die, &child_die, &err)) == DW_DLV_ERROR) {
			printf("Error at dwarf_siblingof in cmd_helper.c@cmdh_print_getFunc\n%s\n", dwarf_errmsg(err));
			return RE_FATAL;
		} else if (rc == DW_DLV_NO_ENTRY) {
			return RE_NON_FATAL;
		}
	}
	*die = child_die;	
	return RE_OK;
}

// Check if a DIE is a function DIE
int cmdh_print_getFunc_findFunc(Dwarf_Die child_die, struct user_regs_struct *regs, int *found) {
	Dwarf_Half tag;
	Dwarf_Attribute *attrs;
	Dwarf_Signed attrcount;
	Dwarf_Addr lowpc;
	Dwarf_Addr highpc;
	Dwarf_Error err;

	int flow = 0, fhigh = 0, i;

	if (dwarf_tag(child_die, &tag, &err) == DW_DLV_ERROR) {
		printf("Error at dwarf_tag in cmd_helper.c@cmdh_print_getFunc_findFunc\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	if (tag != DW_TAG_subprogram) {
		return RE_NON_FATAL;
	}

	if (dwarf_attrlist(child_die, &attrs, &attrcount, &err) == DW_DLV_ERROR) {
		printf("Error at dwarf_attrlist in cmd_helper.c@cmdh_print_getFunc_findFunc\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	for (i = 0; i < attrcount; ++i) {
		Dwarf_Half attrcode;
		if (dwarf_whatattr(attrs[i], &attrcode, &err) == DW_DLV_ERROR) {
			printf("Error at dwarf_whatattr in cmd_helper.c@cmdh_print_getFunc_findFunc\n%s\n", dwarf_errmsg(err));
			return RE_FATAL;
		}
		if (attrcode == DW_AT_low_pc) {
			if (dwarf_formaddr(attrs[i], &lowpc, &err) == DW_DLV_ERROR) {
				printf("Error at dwarf_formaddr lowpc in cmd_helper.c@cmdh_print_getFunc_findFunc\n%s\n", dwarf_errmsg(err));
				return RE_FATAL;
			}
			flow = 1;
		} else if (attrcode == DW_AT_high_pc) {
			if (dwarf_formaddr(attrs[i], &highpc, &err) == DW_DLV_ERROR) {
				printf("Error at dwarf_formaddr highpc in cmd_helper.c@cmdh_print_getFunc_findFunc\n%s\n", dwarf_errmsg(err));
				return RE_FATAL;
			}
			fhigh = 1;
		}
	}

	if (flow && fhigh) {
		if (regs->rip <= highpc && regs->rip >= lowpc)
			*found = 1;
		return RE_OK;
	} else {
		printf("Couldn't not find appropriate DIE.\n");
		return RE_NON_FATAL;
	}
	return RE_OK;
}

// Look through DIEs and check for a right var DIE
int cmdh_print_getVar(mygdb_t *mygdb, Dwarf_Die fun_die, Dwarf_Die *var_die, char *var) {
	int rc;
	Dwarf_Die child_die;
	Dwarf_Error err;

	if (dwarf_child(fun_die, &child_die, &err) == DW_DLV_ERROR) {
		printf("Error at dwarf_child in cmd_helper.c@cmdh_print_getVar\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	while (1) {
		int found = 0;
		if (cmdh_print_getVar_findVar(child_die, &found, var) == RE_FATAL) {
			printf("Can't execute cmdh_print_getVar_findVar in cmd_helper.c@cmdh_print_getVar\n");
			return RE_FATAL;
		}

		if (found) {
			*var_die = child_die;
			return RE_OK;
		}

		if ((rc = dwarf_siblingof(mygdb->t_dbg, child_die, &child_die, &err)) == DW_DLV_ERROR) {
			printf("Error at dwarf_siblingof in cmd_helper.c@cmdh_print_getFunc\n%s\n", dwarf_errmsg(err));
			return RE_FATAL;
		} else if (rc == DW_DLV_NO_ENTRY) {
			printf("Couldn't find any entry\n");
			return RE_NON_FATAL;
		}
	}

	*var_die = child_die;
	return RE_OK;
}

// Check if the DIE is a variable
int cmdh_print_getVar_findVar(Dwarf_Die child_die, int *found, char *var) {
	Dwarf_Half tag;
	Dwarf_Attribute *attrs;
	Dwarf_Signed attrcount;
	Dwarf_Error err;
	int i;

	if (dwarf_tag(child_die, &tag, &err) == DW_DLV_ERROR) {
		printf("Failed at dwarf_tag in cmd_helper.c@cmdh_print_getVar_findVar\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	if (tag != DW_TAG_variable) return RE_NON_FATAL;

	if (dwarf_attrlist(child_die, &attrs, &attrcount, &err) == DW_DLV_ERROR) {
		printf("Failed at dwarf_attrlist in cmd_helper.c@cmdh_print_getVar_findVar\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	for (i = 0; i < attrcount; ++i) {
		Dwarf_Half attrcode;
		if (dwarf_whatattr(attrs[i], &attrcode, &err) == DW_DLV_ERROR) {
			printf("Failed at dwarf_whatattr in cmd_helper.c@cmdh_print_getVar_findVar\n%s\n", dwarf_errmsg(err));
			return RE_FATAL;
		}

		if (attrcode == DW_AT_name) {
			char *die_var_name;
			dwarf_formstring(attrs[i], &die_var_name, &err);
			if (strcmp(die_var_name, var) == 0)
				*found = 1;
			return RE_OK;
		}
	}
	return RE_FATAL;
}

// Print variable with provided DIE
int cmdh_print_printVar(mygdb_t *mygdb, Dwarf_Die die, char *var) {
	Dwarf_Half tag;
	Dwarf_Attribute *attrs;
	Dwarf_Locdesc *locs;
	Dwarf_Signed attrcount, i, count;
	Dwarf_Error err;
	unsigned long addr;

	if (dwarf_tag(die, &tag, &err) != DW_DLV_OK) {
		printf("Error at dwarf_tag() in cmd_helper.c@cmdh_var_print\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	if (tag != DW_TAG_variable) {
		printf("cmd_helper.c@cmdh_var_print: Isn't variable.\n");
		return RE_FATAL;
	}

	if (dwarf_attrlist(die, &attrs, &attrcount, &err) != DW_DLV_OK) {
		printf("Error at dwarf_attrlist() in cmd_helper.c@cmdh_var_print\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	for (i = 0; i < attrcount; ++i) {
		Dwarf_Half attrcode;
		if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
			printf("Error at dwarf_whatattr() in cmd_helper.c@cmdh_var_print\n%s\n", dwarf_errmsg(err));
			return RE_FATAL;
		}

		if (attrcode == DW_AT_location) break;
	}

	if (attrs[i] == NULL) {
		printf("Couldn't find DW_AT_location.\n");
		return RE_NON_FATAL;
	}

	if (dwarf_loclist(attrs[i], &locs, &count, &err) != DW_DLV_OK) {
			printf("Error at dwarf_loclist() in cmd_helper.c@cmdh_var_print\n%s\n", dwarf_errmsg(err));
			return RE_NON_FATAL;
	}

	if ((count != 1) || (locs[0].ld_cents != 1) || (locs[0].ld_s[0].lr_atom != DW_OP_fbreg)) {
		printf("Couldn't find the correct information.\n");
		return RE_FATAL;
	}

	struct user_regs_struct regs;
	if (ptrace(PTRACE_GETREGS, mygdb->child, NULL, &regs) < 0) {
		printf("Failed to get registers in cmd_helper.c@cmdh_var_print\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	addr = (unsigned long) (regs.rbp + (long) locs[0].ld_s[0].lr_number + 16);

	unsigned long data;
	if ((data = ptrace(PTRACE_PEEKTEXT, mygdb->child, (void *) addr)) < 0) {
		printf("Failed to peek text in cmd_helper.c@cmdh_var_print\n%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	printf("%s = %ld\n", var, data);

	return RE_OK;
}
