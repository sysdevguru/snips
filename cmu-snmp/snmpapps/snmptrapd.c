/*
 * Receive SNMP V1 and V2 traps
 */

/**********************************************************************
 *
 *           Copyright 1998 by Carnegie Mellon University
 * 
 *                       All Rights Reserved
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of CMU not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * 
 * CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * 
 **********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <errno.h>
#include <syslog.h>
#include <netdb.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#else /* HAVE_STRINGS_H */
# include <string.h>
#endif /* HAVE_STRINGS_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include <snmp/snmp.h>
#include "version.h"
#include "options.h"

static char rcsid[] = 
"$Id: snmptrapd.c,v 1.12 1998/08/13 01:37:31 ryan Exp $";

extern int  errno;

static int Print = 1;
static int Syslog = 0;

#define PACKET_LENGTH 4500

/* Return the description of this SNMPv1 trap */
char *trap_description(int trap)
{
  switch(trap) {
  case SNMP_TRAP_COLDSTART:
    return "Cold Start";
  case SNMP_TRAP_WARMSTART:
    return "Warm Start";
  case SNMP_TRAP_LINKDOWN:
    return "Link Down";
  case SNMP_TRAP_LINKUP:
    return "Link Up";
  case SNMP_TRAP_AUTHENTICATIONFAILURE:
    return "Authentication Failure";
  case SNMP_TRAP_EGPNEIGHBORLOSS:
    return "EGP Neighbor Loss";
  case SNMP_TRAP_ENTERPRISESPECIFIC:
    return "Enterprise Specific";
  default:
    return "Unknown Type";
  }
}

/* Read a trap from the FD */

void read_trap(int fd)
{
  struct variable_list *vars;
  char buf[PACKET_LENGTH];
  struct snmp_pdu *pdu;

  u_char   packet[PACKET_LENGTH];
  struct sockaddr_in from;
  int      fromlen = sizeof(from);
  int      len;
  u_char *Community;

  char varbuf[4096];

  /* Read the packet */
  len = recvfrom(fd, (char *)packet, PACKET_LENGTH,
                 0, (struct sockaddr *)&from, &fromlen);
  if (len == -1) {
    perror("recvfrom");
    return;
  }

  /* Create an empty pdu */
  pdu = snmp_pdu_create(0);
  if (pdu == NULL) {
    perror("malloc");
    return;
  }
  pdu->address = from;
  pdu->reqid = 0;

  /* Parse the PDU */
  {
    /* Silly hack to get snmp_parse to work. */
    struct snmp_session session; 

    Community = snmp_parse(&session, pdu, packet, len);
    if (Community == NULL) {
      fprintf(stderr, "Mangled packet!\n");
      snmp_free_pdu(pdu);
      return;
    }
    free(Community);
  }

  /* Now that we have a completed PDU, see what it is */
  if (pdu->command == TRP_REQ_MSG) {

    if (Print) {
      printf("***** BEGIN RECEIVED V1 TRAP *****\n");
      printf("%s: %s Trap (%d) Uptime: %s\n",
	     inet_ntoa(pdu->agent_addr.sin_addr),
	     trap_description(pdu->trap_type), 
	     pdu->specific_type,
	     uptime_string(pdu->time, buf));
#ifdef TRAPD_LOG_ENTERPRISE
      {
	int x;
	char foo[1024];
	varbuf[0] = '\0';
	for (x=0; x<pdu->enterprise_length; x++) {
	  sprintf(foo, ".%u", pdu->enterprise[x]);
	  strcat(varbuf, foo);
	}
	printf("Src Enterprise ID %s\n", varbuf);
      }
#endif /* TRAPD_LOG_ENTERPRISE */
      for (vars = pdu->variables; vars; vars = vars->next_variable) {
	print_variable_list(vars);
      }
      printf("***** END OF RECEIVED V1 TRAP *****\n");
    }

    if (Syslog) {
      syslog(LOG_INFO, "***** BEGIN RECEIVED V1 TRAP *****\n");
      syslog(LOG_INFO, "%s: %s Trap (%d) Uptime: %s\n",
	     inet_ntoa(pdu->agent_addr.sin_addr),
	     trap_description(pdu->trap_type), 
	     pdu->specific_type,
	     uptime_string(pdu->time, buf));
#ifdef TRAPD_LOG_ENTERPRISE
      {
	int x;
	char foo[1024];
	varbuf[0] = '\0';
	for (x=0; x<pdu->enterprise_length; x++) {
	  sprintf(foo, ".%u", pdu->enterprise[x]);
	  strcat(varbuf, foo);
	}
	syslog(LOG_INFO, "Src Enterprise ID %s\n", varbuf);
      }
#endif /* TRAPD_LOG_ENTERPRISE */
      for (vars = pdu->variables; vars; vars = vars->next_variable) {
	sprint_variable(varbuf, vars->name, vars->name_length, vars);
	syslog(LOG_INFO, varbuf);
	varbuf[0] = '\0';
      }
      syslog(LOG_INFO, "***** END OF RECEIVED V1 TRAP *****\n");

    }

  } else if (pdu->command == SNMP_PDU_V2TRAP) {

    if (Print) {
      printf("***** BEGIN RECEIVED V2 TRAP *****\n");
      printf("%s sent the following data:\n",
	     inet_ntoa(pdu->address.sin_addr));
      for (vars = pdu->variables; vars; vars = vars->next_variable) {
	print_variable_list(vars);
      }
      printf("***** END RECEIVED V2 TRAP *****\n");
    }

    if (Syslog) {
      syslog(LOG_INFO, "***** BEGIN RECEIVED V2 TRAP *****\n");
      syslog(LOG_INFO, "%s sent the following data:\n",
	     inet_ntoa(pdu->address.sin_addr));
      for (vars = pdu->variables; vars; vars = vars->next_variable) {
	sprint_variable(varbuf, vars->name, vars->name_length, vars);
	syslog(LOG_INFO, varbuf);
	varbuf[0] = '\0';
      }
      syslog(LOG_INFO, "***** END RECEIVED V2 TRAP *****\n");
    }
  } else {
    fprintf(stderr, "Weird!  Received PDU %d", pdu->command);
  }

  snmp_free_pdu(pdu);
}


void init_syslog(void) 
{
  /*
   * These definitions handle 4.2 systems without additional syslog facilities.
   */
#ifndef LOG_CONS
#define LOG_CONS	0	/* Don't bother if not defined... */
#endif
#ifndef LOG_LOCAL0
#define LOG_LOCAL0	0
#endif
    /*
     * All messages will be logged to the local0 facility and will be sent to
     * the console if syslog doesn't work.
     */
  openlog("snmptrapd", LOG_CONS, LOG_LOCAL0);
  syslog(LOG_INFO, "Starting snmptrapd");
}

void Version(void)
{
  fprintf(stderr, "snmptrapd, version %s\n", snmpapps_Version());
  fprintf(stderr, "\tUsing CMU SNMP Library, version %s\n", snmp_Version());
  exit(1);
}

void Usage(void)
{
  fprintf(stderr, "Usage: snmptrapd [-P] [-V] [-S] [-d] [-p port]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Args are:\n");
  fprintf(stderr, "  -d     Dump packets being sent / received\n");
  fprintf(stderr, "  -p P   Set port to P\n");
  fprintf(stderr, "  -V     Print version information\n");
  fprintf(stderr, "  -S     Write received traps to syslog\n");
  exit(1);
}

void main(int argc, char **argv)
{
  int SD, arg, ret;
  fd_set fdset;
  int count, numfds;
  struct sockaddr_in  ME;
  struct servent *servp;
  int Port;

  /* Make stdout unbuffered so we can send it to a file and still be able to
   * see the messages in a timely fashion.
   */
  setbuf(stdout, NULL);

  /* Initialize the MIB */
  init_mib();

  /* Initialize syslog */
  init_syslog();

  servp = getservbyname("snmp-trap", "udp");
  if (servp == NULL)
    Port = SNMP_TRAP_PORT;
  else
    Port = servp->s_port;

  /* usage: snmptrapd [-p] [-s] [-d]
   */
  for(arg = 1; arg < argc; arg++) {
    if (argv[arg][0] == '-') {
      switch(argv[arg][1]) {
      case 'd':
	snmp_dump_packet(1);
	break;
      case 'P':
	Print++;
	break;
      case 'S':
	Syslog++;
	break;
      case 'p':
	Port = atoi(argv[++arg]);
	break;
      case 'V':
	Version();
	break;
      default:
	printf("invalid option: -%c\n", argv[arg][1]);
	Usage();
	break;
      }
      continue;
    }
  }

  /* Open socket */
  SD = socket(AF_INET, SOCK_DGRAM, 0);
  if (SD < 0) {
    perror("socket");
    exit(-1);
  }

  ME.sin_family = AF_INET;
  ME.sin_addr.s_addr = INADDR_ANY;
  ME.sin_port = Port;

  ret = bind(SD, (struct sockaddr *)&ME, sizeof(ME));
  if (ret) {
    perror("bind");
    close(SD);
    exit(-1);
  }
  

  /* Loop and read traps */
  while(1) {
    numfds = 0;
    FD_ZERO(&fdset);

    /* Add trap port */
    numfds = SD + 1;
    FD_SET(SD, &fdset);

    /* Wait for a packet */
    count = select(numfds, &fdset, 0, 0, NULL);

    /* Read it */
    if (count > 0) {
      if (FD_ISSET(SD, &fdset))
	read_trap(SD);
      else {
	fprintf(stderr, "Weird!  select returned something other than what\n");
	fprintf(stderr, "we're waiting for! (%d)\n", count);
      }

    } else switch(count) {
    case 0:
      /* Timeout! */
      break;
    case -1:
      if (errno == EINTR) {
	continue;
      } else {
	perror("select");
      }
      exit(-1);
    default:
      printf("select returned %d\n", count);
      exit(-1);
    }
  } /* End of while loop */
}
