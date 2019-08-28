/*
 * Send an SNMPv2 Trap to a manager
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

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

#include <snmp/snmp.h>

#include "version.h"

static char rcsid[] = 
"$Id: snmptrap2.c,v 1.9 1998/08/06 01:23:09 ryan Exp $";

extern int errno;

oid sysUpTimeOid[9] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
oid snmpTrapOid[11] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 }; /* XXXXX */
/* .iso.org.dod.internet.snmpV2 (.1.3.6.1.6) */
/*                             .3          .1      .1             .4 */
/*                             .snmpModules.snmpMIB.snmpMIBObjects.snmpTrap */

/* Well-known traps are in .1.3.6.1.6.3.1.1.5 (snmpMIBObjects.snmpTraps) */

void parse_text_oid(char *Text, oid *OID, int *Len)
{
  char *bufp = Text;
  char *dotp;
  oid  *oidp = OID;
  int End = 0;

  if (*bufp == '.')
    bufp++;
  *Len = 0;

  while (1) {
    dotp = bufp;
    /* Find next dot */
    while((*dotp != '.') && (*dotp != '\0'))
      dotp++;
    /* NULL terminate */
    if (*dotp == '\0')
      End = 1;
    else
      *dotp = '\0';
    /* Copy num */
    *oidp++ = atoi(bufp);
    *Len = *Len + 1;

    /* Restore */
    if (End) {
      return;
    }
    *dotp = '.';
    bufp = dotp;
    bufp++;
  }
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
  fprintf(stderr, "snmptrap2, version %s\n", snmpapps_Version());
  fprintf(stderr, "\tUsing CMU SNMP Library, version %s\n", snmp_Version());
  exit(1);
}

void Usage(void)
{
  fprintf(stderr, "usage: snmptrap2 [args] host community trap-number uptime\n");
  fprintf(stderr, "       [ [ OID Type Value ] ... ]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Args are:\n");
  fprintf(stderr, "  -d     Dump packets being sent / received\n");
  fprintf(stderr, "  -V     Print version information\n");
  fprintf(stderr, "  -p P   Set port to P\n");
  fprintf(stderr, "  -r R   Set number of retries to R (Default: 4)\n");
  fprintf(stderr, "  -t T   Set timeout to T microseconds (Default: 1000000)\n");

  exit(-1);
}

int main(int argc, char **argv)
{
  struct snmp_pdu *PDU;
  struct variable_list *Var, *Ptr;
  int	arg;
  char *Manager     = NULL;
  char *Community   = NULL; /* Community */
  char *ThisTrap    = NULL; /* Trap Identifier */
  oid   OID[256];           /* Temporary oid holder */
  int Port = SNMP_TRAP_PORT;
  int Retries = SNMP_DEFAULT_RETRIES;
  int Timeout = SNMP_DEFAULT_TIMEOUT;
  time_t Uptime = 0;

  /*
   * usage: snmptrap gateway-name community-name trap-number 
   *
   *         [ [ OID Type Value ] ... ]
   */
  for(arg = 1; arg < argc; arg++) {
    if (argv[arg][0] == '-') {
      switch(argv[arg][1]) {
      case 'r':
	Retries = atoi(argv[++arg]);
	break;
      case 't':
	Timeout = atoi(argv[++arg]);
	break;
      case 'd':
	snmp_dump_packet(1);
	break;
      case 'V':
	Version();
	break;
      case 'p':
	Port = atoi(argv[++arg]);
	break;
      default:
	fprintf(stderr, "invalid option: -%c\n", argv[arg][1]);
	break;
      }
      continue;
    }

    if (Manager == NULL) {
      Manager = argv[arg];
    } else if (Community == NULL) {
      Community = argv[arg]; 
    } else if (ThisTrap == NULL) {
      ThisTrap = argv[arg];
    } else if (Uptime == 0) {
      Uptime = atoi(argv[arg]);
    } else {
      /* If we're pointing to the first optional arg, wait
       * until later to parse it.
       */
      break;
    }
  }

  /* ASSERT:  'arg' now points to the first of the optional
   * arguments.
   */

  /* All args present? */
  if (!(Manager && Community && ThisTrap )) {
    Usage();
    exit(1);
  }

  /* Create the Trap PDU */
  PDU = snmp_pdu_create(SNMP_PDU_V2TRAP);
  if (PDU == NULL)
    exit(-1);
  PDU->reqid = 1;

  /* Who is this from?  By default, us. */
  PDU->agent_addr.sin_addr.s_addr = myaddress();

  /* The first entry is the uptime */
  Var = snmp_var_new(sysUpTimeOid, 9);
  if (Var == NULL)
    exit(-1);
  Var->type = SMI_TIMETICKS;
  Var->val.integer = (int *)malloc(sizeof(int));
  *Var->val.integer = Uptime;
  Var->val_len = sizeof(int);
  PDU->variables = Var;
  Ptr = Var;

  /* The next entry is the OID */
  Var = snmp_var_new(snmpTrapOid, 11);
  if (Var == NULL) {
    snmp_var_free(Ptr);
    exit(-1);
  }
  Var->type = ASN_OBJECT_ID;
  {
    Var->val_len = MAX_NAME_LEN;
    /* Convert ThisTrap into OID */
    parse_text_oid(ThisTrap, OID, &Var->val_len);
    /* Copy OID */
    Var->val_len = Var->val_len * sizeof(oid);
    Var->val.objid = (oid *)malloc(Var->val_len);
    memcpy(Var->val.objid, OID, Var->val_len);
    /* Insert */
    Ptr->next_variable = Var;
    Ptr = Var;
  }

  /* Now insert any other optional OIDs */
  {
    char *TextOid;
    char *Value;
    char *Type;
    int Len;
    
    while (arg != argc) {
      TextOid = argv[arg++];
      Type    = argv[arg++];
      Value   = argv[arg++];

      /* This variable */
      parse_text_oid(TextOid, OID, &Len);
      Var = snmp_var_new(OID, Len);
      if (Var == NULL)
	exit(-1);

      switch(Type[0]) {
      case 'i':
	Var->type = ASN_INTEGER;
	Var->val.integer = (int *)malloc(sizeof(int));
	*(Var->val.integer) = atoi(Value);
	Var->val_len = sizeof(int);
	break;

      case 's':
	Var->type = ASN_OCTET_STR;
	Var->val_len = strlen(Value);
	Var->val.string = (u_char *)malloc(Var->val_len);
	memcpy((char *)Var->val.string, (char *)Value, Var->val_len);
	break;

      case 'n':
	Var->type = ASN_NULL;
	Var->val_len = 0;
	Var->val.string = NULL;
	break;

      case 'o':
	Var->type = ASN_OBJECT_ID;
	Var->val_len = MAX_NAME_LEN;;
	parse_text_oid(Value, OID, &Var->val_len);
	Var->val_len *= sizeof(oid);
	Var->val.objid = (oid *)malloc(Var->val_len);
	memcpy((char *)Var->val.objid, (char *)Value, Var->val_len);
	break;

      case 't':
	Var->type = SMI_TIMETICKS;
	Var->val.integer = (int *)malloc(sizeof(int));
	*(Var->val.integer) = atoi(Value);
	Var->val_len = sizeof(int);
	break;

      case 'a':
	Var->type = SMI_IPADDRESS;
	Var->val.integer = (int *)malloc(sizeof(int));
	*(Var->val.integer) = inet_addr(Value);
	Var->val_len = sizeof(int);
	break;

#if 0
      case 'x':
	Var->type = ASN_OCTET_STR;
	Var->val_len = hex_to_binary((u_char *)newValue, (u_char *)Value);
	Var->val.string = (u_char *)malloc(Var->val_len);
	memcpy((char *)Var->val.string, (char *)Value, Var->val_len);
	break;
      case 'd':
	Var->type = ASN_OCTET_STR;
	Var->val_len = ascii_to_binary((u_char *)newValue, (u_char *)Value);
	Var->val.string = (u_char *)malloc(Var->val_len);
	memcpy((char *)Var->val.string, (char *)Value, Var->val_len);
	break;
#endif
 
      default:
	fprintf(stderr, "Unknown OID type '%c'!\n", Type[0]);
	exit(-1);
	break;
      }
      /* Insert */
      Ptr->next_variable = Var;
      Ptr = Var;
    }
  }


  /* Now send it */
  {
    struct snmp_session session, *ss;

    session.peername = Manager;
    session.Version = SNMP_VERSION_2;
    session.community = (u_char *)Community;
    session.community_len = strlen((char *)Community);
    session.retries     = Retries;
    session.timeout     = Timeout;
    session.remote_port = Port;
    
    ss = snmp_open(&session);
    snmp_send(ss, PDU);
    snmp_close(ss);
    snmp_free_pdu(PDU);
  }

  exit(0);
}

