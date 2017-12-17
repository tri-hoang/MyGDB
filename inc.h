#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <dwarf.h>
#include <libdwarf.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>

#define HIGH_BIT_MASK (~((unsigned long)0xFF))

#define EXEC_FILE		"file"
#define EXEC_RUN		"run"
#define EXEC_QUIT		"quit"
#define EXEC_BREAK		"break"
#define EXEC_CONTINUE 	"continue"
#define EXEC_PRINT		"print"

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
	int   bps_enabled;
	int   bps_active;
} mygdb_t;

typedef enum {
	CMD_END,
	CMD_GO,
	CMD_FILE,
	CMD_RUN,
	CMD_QUIT,
	CMD_BREAK,
	CMD_CONTINUE,
	CMD_PRINT
} cmd_t;

typedef enum {
	R_FATAL,
	R_OK,
	R_NON_FATAL
} return_t;