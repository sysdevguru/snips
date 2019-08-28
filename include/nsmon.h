/*
 * $Header: /home/cvsroot/snips/include/nsmon.h,v 1.0 2001/07/08 22:04:27 vikas Exp $
 */

#ifndef __nsmon_h
# define __nsmon_h

#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/nameser.h>	/* defines all resource types & class */
#include <resolv.h>		/* definition of 'state' */


/*
 * The progam automatically fills in its own name in the EVENT.sender
 * field.
 */

#define VARNM		"named-status"  	/* for EVENT.var.name field */
#define VARUNITS	"SOA"			/* Units name (none)*/
#define POLLINTERVAL	(time_t)60		/* interval between queries */

/*
 * C_IN and T_SOA are #defined in /usr/include/arpa/nameser.h.
 * MAKE SURE THAT arpa/nameser.h has been included at this point.
 *
 * QUERYDATA is the actual data with which to query the server.
 */

#define QUERYTIMEOUT	20	/* timeout connections in this many seconds */
#define QUERYTYPE	T_SOA	/* type of query to make, see arpa/nameser.h */
#define QUERY_AA_ONLY	1	/* set to 1 to set RES_AAONLY flag of _res */
#ifndef QUERYDATA
#  define QUERYDATA	"navya.com"	/* can override in config file */
#endif

/* return codes from nsmon() */

#define ALL_OK			0	/* everything's groovy */
#define NS_ERROR		1	/* no response, or connection */
#define NOT_AUTHORITATIVE	2	/* got a response, but not auth. */
#define NO_NSDATA		3	/* nothing came back in response */

/*
 * return codes from res_query(). These were built from the response codes
 * defined in <arpa/nameser.h>
 */
   
static char *Rcode_Bindings[] = {
  "NOERROR",		/* 0 */
  "FORMERR",		/* 1 */
  "SERVFAIL",		/* 2 */
  "NXDOMAIN",		/* 3 */
  "NOTIMP",		/* 4 */
  "REFUSED",		/* 5 */
  "<unknown rcode>",	/* 6 */
  "<unknown rcode>",	/* 7 */
  "<unknown rcode>",	/* 8 */
  "<unknown rcode>",	/* 9 */
  "<unknown rcode>",	/* A */
  "<unknown rcode>",	/* B */
  "<unknown rcode>",	/* C */
  "<unknown rcode>",	/* D */
  "<unknown rcode>",	/* E */
  "NOCHANGE"		/* F */
};

#endif /* __nsmon_h */


