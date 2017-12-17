#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dwarf.h>
#include <libdwarf.h>
#include <sys/ptrace.h>
#include <sys/user.h>

#define HIGH_BIT_MASK (~((unsigned long)0xFF))

typedef struct {
	Dwarf_Addr addr;
	int line;
	long original;
} bp_t;

typedef struct {
	char *file;
	int fd;
	Dwarf_Debug debug;
	Dwarf_Die cu_die;
	pid_t child;
	bp_t *bps;
	int   bps_count;
	int   bps_enabled; //TODO: Set to -1 init
	int   bps_active;
} mygdb_t;

