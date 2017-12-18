#include "cmd_helper.h"

int cmdh_run_load(mygdb_t *mygdb) {
	Dwarf_Die previous_die = 0;
	Dwarf_Unsigned cu_header_length;
	Dwarf_Unsigned abbrev_offset;
	Dwarf_Unsigned next_cu_header;
	Dwarf_Half version_stamp;
	Dwarf_Half address_size;
	Dwarf_Error err;

	mygdb->fd = open(mygdb->file, O_RDONLY);

	if (dwarf_init(mygdb->fd, DW_DLC_READ, 0, 0, &mygdb->debug, &err) != DW_DLV_OK) {
		printf("Failed at dwarf_init() in cmd_helper.c\n%s\n", strerror(errno));
		return 0;
	}

	if (dwarf_next_cu_header(mygdb->debug, &cu_header_length, &version_stamp, &abbrev_offset, &address_size, &next_cu_header, &err) != DW_DLV_OK) {
		printf("Failed at dwarf_next_cu_header() in cmd_helper.c\n%s\n", strerror(errno));
		return 0;
	}

	if (dwarf_siblingof(mygdb->debug, previous_die, &mygdb->cu_die, &err) != DW_DLV_OK) {
		printf("Failed at dwarf_siblingof() in cmd_helper.c\n");
		return 0;
	}
	return 1;
}

int cmdh_bp_addr(mygdb_t *mygdb, int line_num, Dwarf_Addr *addr) {
	Dwarf_Line *l_buff;
	Dwarf_Unsigned l_num;
	Dwarf_Signed count;
	Dwarf_Error error;
	int i = 0;

	if (dwarf_srclines(mygdb->cu_die, &l_buff, &count, &error) == DW_DLV_ERROR) {
		printf("Failed at dwarf_srclines() in cmd_helper.c\n%s\n", strerror(errno));
		return 0;
	}

	for (i = 0; i < count; i++) {
		printf("%s\n", l_buff[i]);
		if (dwarf_lineno(l_buff[i], &l_num, &error) == DW_DLV_ERROR) {
			printf("Failed at dwarf_lineno() in cmd_helper.c\n%s\n", strerror(errno));
			return 0;
		}
		if (l_num == line_num) {
			if (dwarf_lineaddr(l_buff[i], addr, &error) == DW_DLV_ERROR) {
				printf("Failed at dwarf_lineaddr() in cmd_helper.c\n%s\n", strerror(errno));
			} else break;
		}
	}
	printf("0x%x\n", *addr);
	return 1;
}

int cmdh_bp_enable(mygdb_t *mygdb) {
	// printf("ENABLING %d %d\n", mygdb->bps_enabled, mygdb->bps_count);	
	if (mygdb->bps_enabled == mygdb->bps_count) return CMD_GO;
	if (mygdb->bps_enabled == -1) mygdb->bps_enabled = 0;
	// printf("ENABLING %d %d\n", mygdb->bps_enabled, mygdb->bps_count);
	for (; mygdb->bps_enabled < mygdb->bps_count; mygdb->bps_enabled++) {
		// printf("%d\n", mygdb->bps_enabled);
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
	// printf("ENABLING %d %d\n", mygdb->bps_enabled, mygdb->bps_count);

	return 1;
}

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

int cmdh_var_func(mygdb_t *mygdb, Dwarf_Die *die) {
	Dwarf_Error err;
	Dwarf_Die abc;

	if (dwarf_child(mygdb->cu_die, die, &err) != DW_DLV_OK) {
		printf("Error at dwarf_child in cmd_helper.c@cmdh_var_func\n%s\n", strerror(errno));
		return RE_FATAL;
	}


	struct user_regs_struct regs;
	if (ptrace(PTRACE_GETREGS, mygdb->child, NULL, &regs) < 0) {
		printf("Error at ptrace getregs in cmd_helper.c@cmdh_var_func\n%s\n", strerror(errno));
		return RE_FATAL;
	}
	int i = 0;
	while (1) {
		printf("%d\n", i++);
		int rc, found = 0;
		if (cmdh_var_func_check(*die, regs.rip - 1, &found) == RE_FATAL) {
			printf("Error at cmdh_var_func_check in cmd_helper.c@cmdh_var_func\n");
			return RE_FATAL;
		}

		if (found) {
			return RE_OK;
		}

		// printf("SIB:%d %d %d\n", mygdb->debug == NULL, die == NULL, die == NULL);
		rc = dwarf_siblingof(mygdb->debug, mygdb->cu_die, die, &err);
		if (rc == DW_DLV_ERROR) {
			printf("%s\n", dwarf_errmsg(err));
			printf("Error at dwarf_siblingof 2 in cmd_helper.c@cmdh_var_func\n%s\n", strerror(errno));
			return RE_FATAL;
		} else if (rc == DW_DLV_NO_ENTRY) {
			return RE_NON_FATAL;
		}
	}

	return RE_FATAL;
}

int cmdh_var_func_check(Dwarf_Die die, int rip, int *found) {
	char *die_name = 0;
	Dwarf_Half tag;
	Dwarf_Attribute *attrs;
	Dwarf_Addr lowpc, highpc;
	Dwarf_Signed attrcount, i;
	Dwarf_Error err;
	int rc;
	if ((rc = dwarf_diename(die, &die_name, &err)) != DW_DLV_OK) {
		printf("Failed at dwarf_diename in cmd_helper.c@cmdh_var_func_check\n%s\n", strerror(errno));
		return RE_FATAL;
	} else if (rc == DW_DLV_NO_ENTRY) {
		printf("Can't find any entry in cmd_helper.c@cmdh_var_func_check\n");
		return RE_NON_FATAL;
	}

	if (dwarf_tag(die, &tag, &err) != DW_DLV_OK) {
		printf("Failed at dwarf_tag in cmd_helper.c@cmdh_var_func_check\n");
		return RE_FATAL;
	}

	if (tag != DW_TAG_subprogram) {
		return RE_NON_FATAL;
	}

	if (dwarf_attrlist(die, &attrs, &attrcount, &err) != DW_DLV_OK) {
		printf("Failed at dwarf_attrlist in cmd_helper.c@cmdh_var_func_check\n%s\n", strerror(errno));
		return RE_FATAL;
	}

	for (i = 0; i < attrcount; ++i) {
		Dwarf_Half attrcode;
		if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
			printf("Failed at dwarf_whatattr in cmd_helper.c@cmdh_var_func_check\n%s\n", strerror(errno));
			return RE_FATAL;
		}

		if (attrcode == DW_AT_low_pc) {
			if (dwarf_formaddr(attrs[i], &lowpc, 0) != DW_DLV_OK) {
				printf("Failed at dwarf_formaddr lowpc in cmd_helper.c@cmdh_var_func_check\n");
				return RE_FATAL;
			}
		}
		else if (attrcode == DW_AT_high_pc) {
			if (dwarf_formaddr(attrs[i], &highpc, 0) != DW_DLV_OK) {
				printf("Failed at dwarf_formaddr highpc in cmd_helper.c@cmdh_var_func_check\n");
				return RE_FATAL;
			}
		}
	}

	if (rip >= lowpc && rip <= highpc)
		*found = 1;

	return RE_OK;
}

int cmdh_var_check(Dwarf_Die die, char * var, int * found) {
	char* die_name = 0;
	Dwarf_Half tag;
	Dwarf_Attribute *attrs;
	Dwarf_Signed attrcount, i;
	Dwarf_Error err;
	int rc;
	if ((rc = dwarf_diename(die, &die_name, &err)) == DW_DLV_ERROR) {
		printf("Error at dwarf_diename() in cmd_helper.c@cmdh_var_check\n%s\n", strerror(errno));
		printf("%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	} else if (rc == DW_DLV_NO_ENTRY) return RE_NON_FATAL;

	if (dwarf_tag(die, &tag, &err) != DW_DLV_OK) {
		printf("Error at dwarf_tag() in cmd_helper.c@cmdh_var_check\n%s\n", strerror(errno));
		printf("%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	if (tag != DW_TAG_variable) {
		printf("cmd_helper.c@cmdh_var_check: Isn't variable.\n");
		return RE_NON_FATAL;
	}

	if (dwarf_attrlist(die, &attrs, &attrcount, &err) != DW_DLV_OK) {
		printf("Error at dwarf_attrlist() in cmd_helper.c@cmdh_var_check\n%s\n", strerror(errno));
		printf("%s\n", dwarf_errmsg(err));
		return RE_FATAL;
	}

	for (i = 0; i < attrcount; ++i) {
		char *var_name;
		if (dwarf_formstring(attrs[i], &var_name, &err) != DW_DLV_OK) {
			printf("Error at dwarf_formstring() in cmd_helper.c@cmdh_var_check\n%s\n", strerror(errno));
			printf("%s\n", dwarf_errmsg(err));
			return RE_FATAL;
		}
		if (strcmp(var_name, var) == 0) {
			*found = 1;
			return RE_OK;
		}
	}
	return 1;
}

int cmdh_var_print(mygdb_t *mygdb, Dwarf_Die die, char *var) {
	Dwarf_Half tag;
	Dwarf_Attribute *attrs;
	Dwarf_Locdesc *locs;
	Dwarf_Signed attrcount, i, count;
	Dwarf_Error err;
	unsigned long addr;

	if (dwarf_tag(die, &tag, &err) != DW_DLV_OK) {
		printf("Error at dwarf_tag() in cmd_helper.c@cmdh_var_print\n%s\n", strerror(errno));
		return RE_FATAL;
	}

	if (tag != DW_TAG_variable) {
		printf("cmd_helper.c@cmdh_var_print: Isn't variable.\n");
		return RE_FATAL;
	}

	if (dwarf_attrlist(die, &attrs, &attrcount, &err) != DW_DLV_OK) {
		printf("Error at dwarf_attrlist() in cmd_helper.c@cmdh_var_print\n%s\n", strerror(errno));
		return RE_FATAL;
	}

	for (i = 0; i < attrcount; ++i) {
		Dwarf_Half attrcode;
		if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK) {
			printf("Error at dwarf_whatattr() in cmd_helper.c@cmdh_var_print\n%s\n", strerror(errno));
			return RE_FATAL;
		}

		if (attrcode == DW_AT_location) break;
	}

	if (attrs[i] == NULL) {
		printf("Couldn't find DW_AT_location.\n");
		return RE_NON_FATAL;
	}

	if (dwarf_loclist(attrs[i], &locs, &count, &err) != DW_DLV_OK) {
			printf("Error at dwarf_loclist() in cmd_helper.c@cmdh_var_print\n%s\n", strerror(errno));
			return RE_FATAL;
	}

	if ((count != 1) || (locs[0].ld_cents != 1) || (locs[0].ld_s[0].lr_atom != DW_OP_fbreg)) {
		printf("Couldn't find the correct information.\n");
		return RE_FATAL;
	}

	struct user_regs_struct regs;
	if (ptrace(PTRACE_GETREGS, mygdb->child, NULL, &regs) < 0) {
		printf("Failed to get registers in cmd_helper.c@cmdh_var_print\n%s\n", strerror(errno));
		return RE_FATAL;
	}

	addr = (unsigned long) (regs.rbp + (long) locs[0].ld_s[0].lr_number + 16);

	unsigned long data;
	if ((data = ptrace(PTRACE_PEEKTEXT, mygdb->child, (void *) addr)) < 0) {
		printf("Failed to peek text in cmd_helper.c@cmdh_var_print\n%s\n", strerror(errno));
		return RE_FATAL;
	}

	printf("%s = %ld\n", var, data);

	return RE_OK;
}
