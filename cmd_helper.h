#include "inc.h"

int cmdh_run_load(mygdb_t *);
int cmdh_bp_addr(mygdb_t *, int, Dwarf_Addr *);
int cmdh_bp_enable(mygdb_t *);
int cmdh_restore_instruction(mygdb_t *);
int cmdh_get_breakpoint(mygdb_t *);
int cmdh_var_func(mygdb_t *, Dwarf_Die *);
int cmdh_var_func_check(Dwarf_Die, int, int*);
int cmdh_var_check(Dwarf_Die, char *, int *);
int cmdh_var_print(mygdb_t *, Dwarf_Die, char *);