/*
 * snmpwalk.c - send snmp GETNEXT requests to a network entity, walking 
 * a subtree.
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
 * $Id: snmpwalk.c,v 1.15 1998/09/19 22:28:45 ryan Exp $
 * 
 **********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>

#ifdef WIN32
#include <memory.h>
#include <winsock2.h>
#else  /* WIN32 */
#include <netinet/in.h>
#endif /* WIN32 */

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#ifdef WIN32
#include <snmp/libsnmp.h>
#else /* WIN32 */
#include <snmp/snmp.h>
#endif /* WIN32 */

#include "version.h"

oid objid_mib[] = {1, 3, 6, 1, 2, 1};

void Version(void)
{
  fprintf(stderr, "snmpwalk, version %s\n", snmpapps_Version());
  fprintf(stderr, "\tUsing CMU SNMP Library, version %s\n", snmp_Version());
  exit(1);
}

void Usage(void)
{
  fprintf(stderr, "usage: snmpwalk [args] hostname community oid\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Args are:\n");
  fprintf(stderr, "  -d     Dump packets being sent / received\n");
  fprintf(stderr, "  -V     Print version information\n");
  fprintf(stderr, "  -q     Quiet mode: print oid as numbers\n");
  fprintf(stderr, "  -p P   Set port to P\n");
  fprintf(stderr, "  -r R   Set number of retries to R (Default: 4)\n");
  fprintf(stderr, "  -t T   Set timeout to T microseconds (Default: 1000000)\n");
  fprintf(stderr, "  -1     Send SNMPv1 packet (Default)\n");
  fprintf(stderr, "  -2     Send SNMPv2 packet\n");

  exit(1);
}

int main(int argc, char **argv)
{
    struct snmp_session	session, *ss;
    struct snmp_pdu *pdu, *response;
    struct variable_list *vars;
    int	arg;
    char *gateway = NULL;
    char *community = NULL;
    int gotroot = 0;
    oid	name[32];
    int name_length;
    oid root[MAX_NAME_LEN];
    int	rootlen, count;
    int status;

    int Port    = SNMP_DEFAULT_REMPORT;
    int Retries = SNMP_DEFAULT_RETRIES;
    int Timeout = SNMP_DEFAULT_TIMEOUT;
    int SNMPVersion = SNMP_VERSION_1;
    int Quiet = 0;
  
    init_mib();

#ifdef WIN32
    winsock_startup();
#endif /* WIN32 */

    /*
     * usage: snmpwalk gateway-name community-name [object-id]
     */
    for(arg = 1; arg < argc; arg++){
	if (argv[arg][0] == '-'){
	    switch(argv[arg][1]){
	    case 'd':
	      snmp_dump_packet(1);
	      break;
	    case 's': /* Backwards compatibility -- < v 1.2 */
	    case 'q':
	      Quiet = 1;
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
	      printf("invalid option: -%c\n", argv[arg][1]);
	      break;
	    }
	    continue;
	}
	if (gateway == NULL){
	    gateway = argv[arg];
	} else if (community == NULL){
	    community = argv[arg]; 
	} else {
	  rootlen = MAX_NAME_LEN;
	  if (read_objid(argv[arg], root, &rootlen)) {
	    gotroot = 1;
	  } else {
	    printf("Invalid object identifier: %s\n", argv[arg]);
	  }
	}
    }

    if (gotroot == 0) {
      rootlen = sizeof(objid_mib) / sizeof(oid);
      memcpy(root, objid_mib, sizeof(objid_mib));
      gotroot = 1;
    }

    if (!(gateway && community && gotroot == 1)){
      Usage();
    }

    memset((char *)&session, '\0', sizeof(struct snmp_session));
    session.Version   = SNMPVersion;
    session.peername  = gateway;
    session.community = (u_char *)community;
    session.community_len = strlen((char *)community);
    session.retries     = Retries;
    session.timeout     = Timeout;
    session.remote_port = Port;
    snmp_synch_setup(&session);
    ss = snmp_open(&session);
    if (ss == NULL){
	printf("Couldn't open snmp\n");
	exit(-1);
    }

    memcpy((char *)name, (char *)root, rootlen * sizeof(oid));
    name_length = rootlen;

    while (1) {

	/* Create a new PDU */
	pdu = snmp_pdu_create(SNMP_PDU_GETNEXT);

	snmp_add_null_var(pdu, name, name_length);

    retry:
	status = snmp_synch_response(ss, pdu, &response);

	if (status == STAT_TIMEOUT) {
	  printf("No Response from %s\n", gateway);
	  exit(1);
	} else if (status != STAT_SUCCESS) {
	  printf("An error occurred, Quitting\n");
	  exit(2);
	}

	/* Received a resposne.  Let's look at it. */
	if (response->errstat != SNMP_ERR_NOERROR) {

	  /* SNMPv1 Compatibility: interpret the basics */
	  if (response->errstat == SNMP_ERR_NOSUCHNAME) {
	    printf("End of MIB.\n");
	  } else {

	    /* Error with the packet. */
	    fprintf(stderr, "Error in packet.\n");
	    fprintf(stderr, "Reason: %s\n", snmp_errstring(response->errstat));

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

	  /* Error handling that one, so we don't know where to go next.
	   * Just exit.
	   */
	  exit(3);
	} else {


	  /* Successfully read a response. */

	  /* Loop through all variables */
	  for(vars = response->variables; vars; vars = vars->next_variable) {

	    if (vars->type == SMI_NOSUCHOBJECT) {
	      fprintf(stderr, "No such object: ");
	      for (count=0; count<vars->name_length; count++)
		fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	      exit(4);
	    } 
	    
	    else if (vars->type == SMI_NOSUCHINSTANCE) {
	      fprintf(stderr, "No such instance: ");
	      for (count=0; count<vars->name_length; count++)
		fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	      exit(5);
	    } 

	    else if (vars->type == SMI_ENDOFMIBVIEW) {
	      fprintf(stderr, "End of MIB view.  Requested OID: ");
	      for (count=0; count<vars->name_length; count++)
		fprintf(stderr, ".%u", (unsigned char)vars->name[count]);
	      exit(6);
	    } 

	    /* Good response, and it exists.  Use it. */

	    /* First, see if this is outside of the requested OID tree.
	     */
	    if (vars->name_length < rootlen || 
		memcmp(root, vars->name, rootlen * sizeof(oid))) {
	      fprintf(stdout, "End of requested OID tree.\n");
	      exit(7);
	    }

	    /* Keep track of this for the next pass.
	     */
	    memcpy((char *)name, (char *)vars->name, 
		   vars->name_length * sizeof(oid));
	    name_length = vars->name_length;

	    /* It's valid, so print it. 
	     */
	    if (Quiet) {
	      print_oid_nums(vars->name, vars->name_length);
	      printf(" -> ");
	      print_type(vars);
	      printf(": ");
	      print_variable_list_value(vars);
	      printf("\n");
	    } else {
	      print_variable_list(vars);
	    }

	  } /* End of var processing loop */
	}   /* End of good response */
    }       /* End of while loop */

    /* NOTREACHED */
}


