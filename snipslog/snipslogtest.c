/* $Header: /home/cvsroot/snips/snipslog/snipslogtest.c,v 1.0 2001/07/09 03:16:39 vikas Exp $ */

/*
 * Program to test if snipslogd daemon is working.
 */

/*
 * $Log: snipslogtest.c,v $
 * Revision 1.0  2001/07/09 03:16:39  vikas
 * For SNIPS v1.0
 *
 */

#include <stdio.h>
#define _MAIN_
#include "snips.h"
#undef _MAIN_

int main(argc, argv)
  int argc;
  char **argv;
{
  int	n, written;
  EVENT	v;

  init_event(&v);
  strcpy(v.sender, "logtest");
  v.severity = E_ERROR;
  v.loglevel = E_ERROR;
  v.state = n_TEST;
  strcpy(v.var.name, "testLog");
  strcpy(v.var.units, "message");

  if (argc > 1)
    n = atoi(argv[1]);
  else
    n = 100;
  openeventlog();
  for (written=0; written < n; written++)
  {
    sprintf(v.device.name, "device%d", written);
    sprintf(v.device.addr, "addr%d", written);
    v.var.value = written + 100000;
    if (eventlog(&v) < 0)
    {
      perror("eventlog") ;
      break;
    }
  }
  fprintf(stderr, "DEBUG: %d/%d records written.\n", written, n);

  v.loglevel = E_CRITICAL ;
  eventlog(&v);
  closeeventlog();
  return(0);
}
