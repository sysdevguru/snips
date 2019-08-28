/*
 * Send an SNMPv1 Trap to a manager
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
#include <sys/types.h>
#include <errno.h>

#ifdef WIN32
#include <memory.h>
#include <winsock2.h>
#else  /* WIN32 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif /* WIN32 */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#ifdef WIN32
#include <snmp/libsnmp.h>
#else /* WIN32 */
#include <snmp/snmp.h>
#endif /* WIN32 */

#include "version.h"
#include "uptime.h"

static char rcsid[] = 
"$Id: snmptrap.c,v 1.16 1998/09/19 22:28:44 ryan Exp $";

extern int  errno;


/* Default information in all traps */
oid objid_enterprise[] = { 1, 3, 6, 1, 4, 1, 3, 1, 1 };
oid objid_sysdescr[]   = { 1, 3, 6, 1, 2, 1, 1, 1, 0 };

/* Default callback */
int snmp_input(){
  return 0;
}

u_int parse_address(address)
    char *address;
{
    u_int addr;
    struct sockaddr_in saddr;
    struct hostent *hp;

    if ((addr = inet_addr(address)) != -1)
	return addr;
    hp = gethostbyname(address);
    if (hp == NULL){
	fprintf(stderr, "unknown host: %s\n", address);
	return 0;
    } else {
      memcpy((char *)&saddr.sin_addr, (char *)hp->h_addr, hp->h_length);
	return saddr.sin_addr.s_addr;
    }

}

void Version(void)
{
  fprintf(stderr, "snmptrap, version %s\n", snmpapps_Version());
  fprintf(stderr, "\tUsing CMU SNMP Library, version %s\n", snmp_Version());
  exit(1);
}

void Usage(void)
{
  fprintf(stderr, "usage: snmptrap [args] host community trap-type specific-type\n");
  fprintf(stderr, "                device-description\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Args are:\n");
  fprintf(stderr, "  -a     Agent address (hostname or IP)\n");
  fprintf(stderr, "  -d     Dump packets being sent / received\n");
  fprintf(stderr, "  -V     Print version information\n");
  fprintf(stderr, "  -p P   Set destination port to P\n");
  fprintf(stderr, "  -r R   Set number of retries to R (Default: 4)\n");
  fprintf(stderr, "  -t T   Set timeout to T microseconds (Default: 1000000)\n");
    exit(1);
}

int main(int argc, char **argv)
{
  struct snmp_session session, *ss;
  struct snmp_pdu *pdu;
  struct variable_list *vars;
  int	arg;
  int ret;
  char *gateway     = NULL; /* Gateway Name */
  char *community   = NULL; /* Community */
  char *trap        = NULL; /* Trap Identifier */
  char *specific    = NULL; /* Enterprise Specific OID */
  char *description = NULL;
  char *agent       = NULL;
  int Port          = SNMP_TRAP_PORT;
  int Retries       = SNMP_DEFAULT_RETRIES;
  int Timeout       = SNMP_DEFAULT_TIMEOUT;

  /*
   * usage: snmptrap gateway-name community-name trap-type specific-type 
   *                 device-description [ -a agent-addr ]
   */
  for(arg = 1; arg < argc; arg++) {
    if (argv[arg][0] == '-') {
      switch(argv[arg][1]) {
      case 'a':
	agent = argv[++arg];
	break;
      case 'd':
	snmp_dump_packet(1);
	break;
      case 'p':
	Port = atoi(argv[++arg]);
	break;
      case 'r':
	Retries = atoi(argv[++arg]);
	break;
      case 't':
	Timeout = atoi(argv[++arg]);
	break;
      case 'V':
	Version();
	break;
      default:
	printf("invalid option: -%c\n", argv[arg][1]);
	break;
      }
      continue;
    }

    if (gateway == NULL) {
      gateway = argv[arg];
    } else if (community == NULL) {
      community = argv[arg]; 
    } else if (trap == NULL) {
      trap = argv[arg];
    } else if (specific == NULL) {
      specific = argv[arg];
    } else {
      description = argv[arg];
    }
  }

#ifdef WIN32
  winsock_startup();
#endif /* WIN32 */

  /* All args present? */
  if (!(gateway && community && trap && specific && description)) {
    Usage();
  }

  memset((char *)&session, '\0', sizeof(struct snmp_session));
  session.peername = gateway;
  session.Version = SNMP_VERSION_1;
  session.community = (u_char *)community;
  session.community_len = strlen((char *)community);
  session.retries = Retries;
  session.timeout = Timeout;

  /* Traps must have a callback */
  session.callback = snmp_input;
  session.callback_magic = NULL;

  /* Send this to the trap port */
  session.remote_port = Port;

  ss = snmp_open(&session);
  if (ss == NULL){
    printf("Couldn't open snmp\n");
    exit(-1);
  }

  /* Create a PDU */
  pdu = snmp_pdu_create(TRP_REQ_MSG);

  /* Set the trap identifier */
  pdu->enterprise = (oid *)malloc(sizeof(objid_enterprise));
  memcpy((char *)pdu->enterprise, (char *)objid_enterprise, 	
	 sizeof(objid_enterprise));
  pdu->enterprise_length = sizeof(objid_enterprise) / sizeof(oid);

  /* Who is this from?  By default, us. */
  if (agent != NULL)
    pdu->agent_addr.sin_addr.s_addr = parse_address(agent);
  else
    pdu->agent_addr.sin_addr.s_addr = myaddress();

  /* Set the trap type */
  pdu->trap_type = atoi(trap);
  pdu->specific_type = atoi(specific);

  /* And the uptime */
  pdu->time = (uptime() * 100); /* In Centi-Seconds */

  /* Add the other necessary variables */
  pdu->variables = snmp_var_new(objid_sysdescr, 9);
  vars = pdu->variables;
  vars->next_variable = NULL;
  vars->type = ASN_OCTET_STR;
  vars->val.string = (u_char *)malloc(strlen(description) + 1);
  strcpy((char *)vars->val.string, description);
  vars->val_len = strlen(description);

  /* Send it */
  ret = snmp_send(ss, pdu);
  if (ret == 0) {
    printf("error\n");
  }
  snmp_close(ss);

  return 0;
}

