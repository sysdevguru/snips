#ifndef EVENT_UTILS_H
#define EVENT_UTILS_H
#include "snips.h"

char *event_to_logstr(EVENT *v);
char **event2strarray(EVENT *pv);
EVENT *strarray2event(char *sap[]);
int init_event(EVENT *pv);
int read_event(int fd, EVENT *pv);
int read_n_events(int fd, EVENT *pv, int n);
int write_event(int fd, EVENT *pv);
int rewrite_event(int fd, EVENT *pv);
int rewrite_n_events(int fd, EVENT *pv, int n);
int update_event(EVENT *pv, int status, u_long value, u_long thres, int maxsev);
int calc_status(u_long val, u_long warnt, u_long errt, u_long critt, int crit_is_hi, u_long *pthres, int *pmaxseverity);

#endif
