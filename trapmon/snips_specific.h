#ifndef SNIPS_SPECIFIC_H
#define SNIPS_SPECIFIC_H

int add_snips_event(char *addr, char *name, struct snmp_pdu *pdu);
int init_snips();
int expire_snips_events();

#endif
