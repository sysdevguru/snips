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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include <snmp/snmp.h>

static char rcsid[] = 
"$Id: snmpset.c,v 1.12 1998/08/06 01:23:07 ryan Exp $";

extern int errno;

int ascii_to_binary(u_char *cp, u_char *bufp)
{
  int	subidentifier;
  u_char *bp = bufp;

  for(; *cp != '\0'; cp++) {
    if (isspace(*cp))
      continue;
    if (!isdigit(*cp)) {
      fprintf(stderr, "Input error\n");
      return -1;
    }
    subidentifier = atoi((char *)cp);
    if (subidentifier > 255) {
      fprintf(stderr, "subidentifier %d is too large ( > 255)\n", subidentifier);
      return -1;
    }
    *bp++ = (u_char)subidentifier;
    while(isdigit(*cp))
      cp++;
    cp--;
  }
  return bp - bufp;
}

int hex_to_binary(u_char *cp, u_char *bufp)
{
  int	subidentifier;
  u_char *bp = bufp;

  for(; *cp != '\0'; cp++) {
    if (isspace(*cp))
      continue;
    if (!isxdigit(*cp)) {
      fprintf(stderr, "Input error\n");
      return -1;
    }
    sscanf((char *)cp, "%x", &subidentifier);
    if (subidentifier > 255) {
      fprintf(stderr, "subidentifier %d is too large ( > 255)\n", subidentifier);
      return -1;
    }
    *bp++ = (u_char)subidentifier;
    while(isxdigit(*cp))
      cp++;
    cp--;
  }
  return bp - bufp;
}

void main(int argc, char **argv)
{
  char *gateway = NULL;
  char *community = NULL;
  char *name = NULL;
  char *type = NULL;
  char *newvalue = NULL;
  oid  value[256];     /* Requested OID * */
  struct variable_list *vp, *vars;
  struct snmp_session session, *ss;
  struct snmp_pdu *pdu, *response;
  int	arg;
  int	    status, count;
  int Port    = SNMP_DEFAULT_REMPORT;
  int Retries = SNMP_DEFAULT_RETRIES;
  int Timeout = SNMP_DEFAULT_TIMEOUT;

  /* Confirm usage
   *
   * usage: snmpset [ -d ] gateway-name community-name oid type value
   *
   */
  for(arg = 1; arg < argc; arg++) {
    if (argv[arg][0] == '-') {
      switch(argv[arg][1]) {
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
      default:
	fprintf(stderr, "invalid option: -%c\n", argv[arg][1]);
	break;
      }
      continue;
    }
    if (gateway == NULL) {
      gateway = argv[arg];
    } else if (community == NULL) {
      community = argv[arg]; 
    } else if (name == NULL) {
      name = argv[arg];
    } else if (type == NULL) {
      type = argv[arg];
    } else if (newvalue == NULL) {
      newvalue = argv[arg];
    } else {
      fprintf(stderr, "usage: snmpset [ -d ] [-p port] gateway-name community-name oid type value\n");
      exit(1);
    }
  }

  /* Confirm args */
  if (!(gateway && community && name && type)) {
      fprintf(stderr, "usage: snmpset [ -d ] [-p port] gateway-name community-name oid type value\n");
    exit(1);
  }

  /* Initialize this session */
  memset((char *)&session, '\0', sizeof(struct snmp_session));
  session.peername = gateway;
  session.community = (u_char *)community;
  session.community_len = strlen((char *)community);
  session.retries     = Retries;
  session.timeout     = Timeout;
  session.remote_port = Port;

  /* Initialize the variable list */
  vp = (struct variable_list *)malloc(sizeof(struct variable_list));
  vp->next_variable = NULL;
  vp->name = NULL;
  vp->val.string = NULL;

  /* Initialize the MIB 
   */
  init_mib();

  /* Did the user request a usable OID? */
  vp->name_length = MAX_NAME_LEN;
  vp->name = (oid *)malloc(sizeof(oid) * vp->name_length);
  if (!read_objid(name, vp->name, &vp->name_length))
    exit(-1);

  /* Set the OID type */
  switch(type[0]) {
  case 'i':
    vp->type = INTEGER;
    vp->val.integer = (int *)malloc(sizeof(int));
    *(vp->val.integer) = atoi(newvalue);
    vp->val_len = sizeof(int);
    break;
  case 's':
    vp->type = STRING;
    strcpy((char *)value, newvalue);
    vp->val_len = strlen(newvalue);
    vp->val.string = (u_char *)malloc(vp->val_len);
    memcpy((char *)vp->val.string, (char *)value, vp->val_len);
    break;
  case 'x':
    vp->type = STRING;
    vp->val_len = hex_to_binary((u_char *)newvalue, (u_char *)value);
    vp->val.string = (u_char *)malloc(vp->val_len);
    memcpy((char *)vp->val.string, (char *)value, vp->val_len);
    break;
  case 'd':
    vp->type = STRING;
    vp->val_len = ascii_to_binary((u_char *)newvalue, (u_char *)value);
    vp->val.string = (u_char *)malloc(vp->val_len);
    memcpy((char *)vp->val.string, (char *)value, vp->val_len);
    break;
  case 'n':
    vp->type = NULLOBJ;
    vp->val_len = 0;
    vp->val.string = NULL;
    break;
  case 'o':
    vp->type = OBJID;
    vp->val_len = MAX_NAME_LEN;;
    vp->val.objid = (oid *)malloc(sizeof(oid) * vp->val_len);
    read_objid(newvalue, vp->val.objid, &(vp->val_len));
    vp->val_len *= sizeof(oid);
    break;
  case 't':
    vp->type = SMI_TIMETICKS;
    vp->val.integer = (int *)malloc(sizeof(int));
    *(vp->val.integer) = atoi(newvalue);
    vp->val_len = sizeof(int);
    break;
  case 'a':
    vp->type = SMI_IPADDRESS;
    vp->val.integer = (int *)malloc(sizeof(int));
    *(vp->val.integer) = inet_addr(newvalue);
    vp->val_len = sizeof(int);
    break;
  default:
    fprintf(stderr, "bad type \"%c\", use \"i\", \"s\", \"x\", \"d\", \"n\", \"o\", \"t\", or \"a\".\n", type[0]);
    exit(-1);
  }

  /* Setup Session */
  snmp_synch_setup(&session);
  ss = snmp_open(&session);
  if (ss == NULL) {
    fprintf(stderr, "Unable to establish connection to %s\n", gateway);
    exit(-1);
  }

  /* Create this PDU */
  pdu = snmp_pdu_create(SNMP_PDU_SET);
  pdu->variables = vp;

  /* Send it */
  status = snmp_synch_response(ss, pdu, &response);
  if (status == STAT_SUCCESS) {
    switch (response->command) {
      case SNMP_PDU_GET:
	printf("Received GET REQUEST ");
	break;
      case SNMP_PDU_GETNEXT:
	printf("Received GETNEXT REQUEST ");
	break;
      case SNMP_PDU_RESPONSE:
	printf("Received GET RESPONSE ");
	break;
      case SNMP_PDU_SET:
	printf("Received SET REQUEST ");
	break;
      case TRP_REQ_MSG:
	printf("Received TRAP REQUEST ");
	break;
      }
    printf("from %s\n", inet_ntoa(response->address.sin_addr));
    printf("requestid 0x%x errstat 0x%x errindex 0x%x\n",
	   (unsigned int)response->reqid, (unsigned int)response->errstat, 
	   (unsigned int)response->errindex);
    if (response->errstat == SNMP_ERR_NOERROR) {
      for(vars = response->variables; vars; vars = vars->next_variable)
	print_variable_list(vars);
    } else {
      fprintf(stderr, "Error in packet.\nReason: %s\n", 
	      snmp_errstring(response->errstat));
      if (response->errstat == SNMP_ERR_NOSUCHNAME) {
	for(count = 1, 
	      vars = response->variables; vars && count != response->errindex;
	      vars = vars->next_variable, count++)
	    ;
	  if (vars) {
	    printf("This name doesn't exist: ");
	    print_oid_nums(vars->name, vars->name_length);
	  }
	  printf("\n");
	}
      }

    } else if (status == STAT_TIMEOUT) {
      fprintf(stderr, "No Response from %s\n", gateway);
    } else {    /* status == STAT_ERROR */
      fprintf(stderr, "An error occurred, Quitting\n");
    }

    if (response)
      snmp_free_pdu(response);
}
