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