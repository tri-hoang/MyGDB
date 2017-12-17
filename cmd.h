#include "cmd_helper.h"

cmd_t cmd_file(mygdb_t *, char *);
cmd_t cmd_run(mygdb_t *);
cmd_t cmd_continue(mygdb_t *);
cmd_t cmd_break(mygdb_t *, int);
cmd_t cmd_quit();
cmd_t cmd_print(mygdb_t *, char *);