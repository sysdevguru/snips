#ifndef SNIPS_FUNCS_H
#define SNIPS_FUNCS_H
#include "snips.h"

#define SNIPS_HOST_MAX_COUNT 10

char **get_snips_loghosts();
char *set_datafile(), *set_configfile();
char *get_datadir(),  *get_etcdir();
char *set_datafile(), *set_configfile();
int snips_startup();
int snips_help();
int snips_reload(int(*newreadconfig)());
int open_datafile(char *filename, int flags);
int close_datafile(int fd);
int read_global_config();
int standalone(char *pidfile);
int copy_events_datafile(char *ofile, char *nfile);
int read_dataversion(int fd);
int extract_dataversion(EVENT *pv);
int check_configfile_age();
void snips_done(), hup_handler(), usr1_handler();    /* for signals */


#endif
