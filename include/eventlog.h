#ifndef EVENTLOG_H
#define EVENTLOG_H
#include "snips.h"

int openeventlog();
int closeeventlog();
int eventlog(EVENT *vin);
void ntohevent(EVENT *f, EVENT *t);
void htonevent(EVENT *f, EVENT *t);

#endif
