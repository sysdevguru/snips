/*
 * snmpstatus.c - Fetch agent status information via SNMP
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
 * $Id: snmpstatus.c,v 1.16 1998/09/19 22:28:42 ryan Exp $
 * 
 **********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <sys/types.h>

#ifdef WIN32
#include <memory.h>
#include <winsock2.h>
#else  /* WIN32 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* WIN32 */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
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

/* A few defines */
#define MIB_IFSTATUS_UP         1
#define MIB_IFSTATUS_DOWN       2
#define MIB_IFSTATUS_TESTING    3

#define OIDCMP(a,alen,b,blen) ((alen==blen)&&(!memcmp(a,b,(blen*sizeof(oid)))))

/* The object's we're requesting */

oid objid_sysDescr[]        = {1, 3, 6, 1, 2, 1, 1, 1, 0};
int NumOidsInSysDescr         = sizeof(objid_sysDescr)/sizeof(oid);
oid objid_sysUpTime[]       = {1, 3, 6, 1, 2, 1, 1, 3, 0};
int NumOidsInSysUpTime        = sizeof(objid_sysUpTime)/sizeof(oid);
oid objid_ifOperStatus[]    = {1, 3, 6, 1, 2, 1, 2, 2, 1, 8};
int NumOidsInIfOperStatus     = sizeof(objid_ifOperStatus)/sizeof(oid);
oid objid_ifInUCastPkts[]   = {1, 3, 6, 1, 2, 1, 2, 2, 1, 11};
int NumOidsInIfInUCastPkts    = sizeof(objid_ifInUCastPkts)/sizeof(oid);
oid objid_ifInNUCastPkts[]  = {1, 3, 6, 1, 2, 1, 2, 2, 1, 12};
int NumOidsInIfInNUCastPkts   = sizeof(objid_ifInNUCastPkts)/sizeof(oid);
oid objid_ifOutUCastPkts[]  = {1, 3, 6, 1, 2, 1, 2, 2, 1, 17};
int NumOidsInIfOutUCastPkts   = sizeof(objid_ifOutUCastPkts)/sizeof(oid);
oid objid_ifOutNUCastPkts[] = {1, 3, 6, 1, 2, 1, 2, 2, 1, 18};
int NumOidsInIfOutNUCastPkts  = sizeof(objid_ifOutNUCastPkts)/sizeof(oid);
oid objid_ipInReceives[]    = {1, 3, 6, 1, 2, 1, 4, 3, 0};
int NumOidsInIpInReceives     = sizeof(objid_ipInReceives)/sizeof(oid);
oid objid_ipOutRequests[]   = {1, 3, 6, 1, 2, 1, 4, 10, 0};
int NumOidsInIpOutRequests    = sizeof(objid_ipOutRequests)/sizeof(oid);

void Version(void)
{
  fprintf(stderr, "snmpstatus, version %s\n", snmpapps_Version());
  fprintf(stderr, "\tUsing CMU SNMP Library, version %s\n", snmp_Version());
  exit(1);
}

void Usage(void)
{
  fprintf(stderr, "usage: snmpstatus [args] hostname [community]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Args are:\n");
  fprintf(stderr, "  -d     Dump packets being sent / received\n");
  fprintf(stderr, "  -V     Print version information\n");
  fprintf(stderr, "  -p P   Set port to P\n");
  fprintf(stderr, "  -r R   Set number of retries to R (Default: 4)\n");
  fprintf(stderr, "  -t T   Set timeout to T microseconds (Default: 1000000)\n");
  fprintf(stderr, "  -1     Send SNMPv1 packet (Default)\n");
  fprintf(stderr, "  -2     Send SNMPv2 packet\n");

  exit(1);
}

int main(int argc, char **argv)
{
  struct snmp_session session, *ss;
  struct snmp_pdu *pdu, *response;
  struct variable_list *vars;
  int	arg;
  char *gateway = NULL;
  char *community = NULL;
  char    name[256];
  char    buf[64];
  int	    good_var, index;
  int	    status, count;
  u_int  ipackets = 0, opackets = 0, down_interfaces = 0;
  u_int  ipin = 0, ipout = 0;
  time_t uptime = 0;

  int Port    = SNMP_DEFAULT_REMPORT;
  int Retries = SNMP_DEFAULT_RETRIES;
  int Timeout = SNMP_DEFAULT_TIMEOUT;
  int SNMPVersion = SNMP_VERSION_1;

  init_mib();

#ifdef WIN32
  winsock_startup();
#endif /* WIN32 */

  /*
   * usage: snmpstatus gateway-name [community-name]
   */
  for (arg = 1; arg < argc; arg++) {
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
      case 'p':
	Port = atoi(argv[++arg]);
	break;
      case 'V':
	Version();
	break;
      case '1':
	SNMPVersion = SNMP_VERSION_1;
	break;
      case '2':
	SNMPVersion = SNMP_VERSION_2;
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
    } else {
      Usage();
    }
  }
  if (!(gateway)) {
    Usage();
  }

  /* Initialize this session */
  memset((char *)&session, '\0', sizeof(struct snmp_session));
  session.Version   = SNMPVersion;
  session.peername  = gateway;
  session.community = (u_char *)community;
  if (community == NULL) {
    session.community_len = SNMP_DEFAULT_COMMUNITY_LEN;
  } else {
    session.community_len = strlen(community);
  }
  session.retries = Retries;
  session.timeout = Timeout;
  session.remote_port = Port;
  snmp_synch_setup(&session);

  /* Open the session */
  ss = snmp_open(&session);
  if (ss == NULL) {
    printf("Couldn't open snmp\n");
    exit(-1);
  }

  /* Create request PDU */
  strcpy(name, "No System Description Available");
  pdu = snmp_pdu_create(SNMP_PDU_GET);

  /* Request this information */
  snmp_add_null_var(pdu, objid_sysDescr,      NumOidsInSysDescr);
  snmp_add_null_var(pdu, objid_sysUpTime,     NumOidsInSysUpTime);
  snmp_add_null_var(pdu, objid_ipInReceives,  NumOidsInIpInReceives);
  snmp_add_null_var(pdu, objid_ipOutRequests, NumOidsInIpOutRequests);

  /* Send and read the response */
retry:

  /* This will free pdu */
  status = snmp_synch_response(ss, pdu, &response);
  pdu = NULL;

  if (status == STAT_TIMEOUT) {
    printf("No Response from %s\n", gateway);
    exit(1);
  } else if (status != STAT_SUCCESS) {
    printf("An error occurred, Quitting\n");
    exit(2);
  }

  if (response->errstat != SNMP_ERR_NOERROR) {

    /* Error with the packet. */
    fprintf(stderr, "Error in packet.\n");
    fprintf(stderr, "Reason: %s\n", snmp_errstring(response->errstat));

    /* SNMPv1 Compatibility: interpret the basics */
    if (response->errstat == SNMP_ERR_NOSUCHNAME) {

      /* Find the variable specified by errindex */
      for(count = 1, vars = response->variables; 
	  vars && count != response->errindex;
	  vars = vars->next_variable, count++)
	  ;
      
      if (vars) {
	fprintf(stderr, "- ");
	for (count=0; count<vars->name_length; count++)
	  fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	fprintf(stderr, "\n");
      }

      /* Remove the bad entry, and try again. */
      pdu = snmp_fix_pdu(response, SNMP_PDU_GET);
      snmp_free_pdu(response);

      if (pdu != NULL)
	goto retry;
    }
  } else {

    /* Successfully read a response. */
    
    /* Loop through all variables */
    for (vars = response->variables; vars; vars = vars->next_variable) {

      /* This shouldn't happen, as these items are pretty basic, but
       * just in case...
       */
      if (vars->type == SMI_NOSUCHOBJECT) {
	fprintf(stderr, "No such object: ");
	for (count=0; count<vars->name_length; count++)
	  fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
      } 

      else if (vars->type == SMI_NOSUCHINSTANCE) {
	fprintf(stderr, "No such instance: ");
	for (count=0; count<vars->name_length; count++)
	  fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
      } 

      else if (vars->type == SMI_ENDOFMIBVIEW) {
	fprintf(stderr, "End of MIB view.  Requested OID: ");
	for (count=0; count<vars->name_length; count++)
	  fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
      } 

      /* Now check for what we requested */
      else if (OIDCMP(vars->name, vars->name_length,
		      objid_sysDescr, NumOidsInSysDescr)) {
	memcpy(name, (char *)vars->val.string, vars->val_len);
	name[vars->val_len] = '\0';
      }

      else if (OIDCMP(vars->name, vars->name_length,
		      objid_sysUpTime, NumOidsInSysUpTime)) {
	uptime = *vars->val.integer;
      }

      else if (OIDCMP(vars->name, vars->name_length,
		      objid_ipInReceives, NumOidsInIpInReceives)) {
	ipin = *vars->val.integer;
      }

      else if (OIDCMP(vars->name, vars->name_length,
		      objid_ipOutRequests, NumOidsInIpOutRequests)) {
	ipout = *vars->val.integer;
      }
    }
    
  }

  /* Print this information. */
  printf("[%s]=>[%s] Up: %s\n", 
	 (char *)inet_ntoa(response->address.sin_addr), 
	 name,
	 uptime_string(uptime, buf));

  if (response)
    snmp_free_pdu(response);

  /* Now find out info about the interfaces */
  pdu = snmp_pdu_create(SNMP_PDU_GETNEXT);

  snmp_add_null_var(pdu, objid_ifOperStatus,    NumOidsInIfOperStatus);
  snmp_add_null_var(pdu, objid_ifInUCastPkts,   NumOidsInIfInUCastPkts);
  snmp_add_null_var(pdu, objid_ifInNUCastPkts,  NumOidsInIfInNUCastPkts);
  snmp_add_null_var(pdu, objid_ifOutUCastPkts,  NumOidsInIfOutUCastPkts);
  snmp_add_null_var(pdu, objid_ifOutNUCastPkts, NumOidsInIfOutNUCastPkts);

  /* Loop through all interfaces */
  good_var = 5;
  while(good_var == 5) {
    good_var = 0;
    status = snmp_synch_response(ss, pdu, &response);
    pdu = NULL;

    if (status == STAT_TIMEOUT) {
      printf("No Response from %s\n", gateway);

    } else if (status == STAT_SUCCESS) {

      if (response->errstat != SNMP_ERR_NOERROR) {
	
	/* Error with the packet. */
	fprintf(stderr, "Error in packet.\n");
	fprintf(stderr, "Reason: %s\n", snmp_errstring(response->errstat));

	/* SNMPv1 Compatibility: interpret the basics */
	if (response->errstat == SNMP_ERR_NOSUCHNAME) {

	  /* Find the variable specified by errindex */
	  for(count = 1, vars = response->variables; 
	      vars && count != response->errindex;
	      vars = vars->next_variable, count++)
	    ;
      
	  if (vars) {
	    fprintf(stderr, "- ");
	    for (count=0; count<vars->name_length; count++)
	      fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	    fprintf(stderr, "\n");
	  }

	}
      } else {

	/* Packet indicated no error.*/
	pdu = snmp_pdu_create(SNMP_PDU_GETNEXT);
	index = 0;

	/* Loop through all variables */
	for(vars = response->variables; vars; vars = vars->next_variable) {

	  if (vars->type == SMI_NOSUCHOBJECT) {
	    fprintf(stderr, "No such object: ");
	    for (count=0; count<vars->name_length; count++)
	      fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	  } 

	  else if (vars->type == SMI_NOSUCHINSTANCE) {
	    fprintf(stderr, "No such instance: ");
	    for (count=0; count<vars->name_length; count++)
	      fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	  } 

	  else if (vars->type == SMI_ENDOFMIBVIEW) {
	    fprintf(stderr, "End of MIB view.  Requested OID: ");
	    for (count=0; count<vars->name_length; count++)
	      fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	  } 

	  /* Now check for what we requested */
	  else if (index == 0 && vars->name_length >= NumOidsInIfOperStatus &&
		   !memcmp((char *)objid_ifOperStatus, (char *)vars->name,
			   sizeof(objid_ifOperStatus))) {
	    if (*vars->val.integer != MIB_IFSTATUS_UP)
	      down_interfaces++;
	    snmp_add_null_var(pdu, vars->name, vars->name_length);
	    good_var++;
	  } 

	  else if (index == 1 && vars->name_length >= NumOidsInIfInUCastPkts &&
		   !memcmp((char *)objid_ifInUCastPkts, (char *)vars->name,
			   sizeof(objid_ifInUCastPkts))) {
	    ipackets += *vars->val.integer;
	    snmp_add_null_var(pdu, vars->name, vars->name_length);
	    good_var++;
	  } 

	  else if (index == 2 && vars->name_length >= NumOidsInIfInNUCastPkts &&
		   !memcmp((char *)objid_ifInNUCastPkts, (char *)vars->name,
			   sizeof(objid_ifInNUCastPkts))) {
	    ipackets += *vars->val.integer;
	    snmp_add_null_var(pdu, vars->name, vars->name_length);
	    good_var++;
	  } 

	  else if (index == 3 && vars->name_length >= NumOidsInIfOutUCastPkts &&
		   !memcmp((char *)objid_ifOutUCastPkts, (char *)vars->name,
			   sizeof(objid_ifOutUCastPkts))) {
	    opackets += *vars->val.integer;
	    snmp_add_null_var(pdu, vars->name, vars->name_length);
	    good_var++;
	  }
	  else if (index == 4 && vars->name_length >= NumOidsInIfOutNUCastPkts &&
		   !memcmp((char *)objid_ifOutNUCastPkts, (char *)vars->name,
			   sizeof(objid_ifOutNUCastPkts))) {
	    opackets += *vars->val.integer;
	    snmp_add_null_var(pdu, vars->name, vars->name_length);
	    good_var++;
	  }
	  index++;
	}
      }
    } else {    /* status == STAT_ERROR */
      printf("An error occurred, Quitting\n");
    }

    if (response)
      snmp_free_pdu(response);
  } /* End (good_var == 5) */

  if (pdu)
    snmp_free_pdu(pdu);

  /* Print what was learned */
  printf("Recv/Trans packets: Interfaces: %d/%d | IP: %d/%d\n", 
	 ipackets, opackets, ipin, ipout);
  if (down_interfaces > 0) {
    printf("%d interface%s down!\n", 
	   down_interfaces, 
	   down_interfaces > 1 ? "s are": " is" );
  }
  exit(0);
}
