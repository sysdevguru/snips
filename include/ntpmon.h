/* $Header: /home/cvsroot/snips/include/ntpmon.h,v 1.1 2008/04/25 23:31:50 tvroon Exp $ */

#ifndef __ntpmon_h
#define __ntpmon_h

/*
 * These definitions have been derived from ntp.h, ntp_control.h, 
 * and other header files from the BSD ntpq package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#if defined(AIX) || defined(_AIX)
# include <sys/select.h>
#endif

/* ntp.h */

#ifndef U_LONG
#define U_LONG u_int
#endif /* U_LONG */
#define NTP_PORT 123
#define MIN_MAC_LEN (sizeof(U_LONG) + 8)        /* DES */
#define MAX_MAC_LEN (sizeof(U_LONG) + 16)       /* MD5 */

#define CTL_ISMORE(r_m_e_op)    (((r_m_e_op) & 0x20) != 0)
#define CTL_ISERROR(r_m_e_op)   (((r_m_e_op) & 0x40) != 0)

struct ntp_control 
{
	u_char li_vn_mode;		/* leap, version, mode */
	u_char r_m_e_op;		/* response, more, error, opcode */
	u_short sequence;		/* sequence number of request */
	u_short status;			/* status word for association */
	u_short associd;		/* association ID */
	u_short offset;			/* offset of this batch of data */
	u_short count;			/* count of data in this packet */
	u_char data[(480 + MAX_MAC_LEN)]; /* data + auth */
};

/*
 * Stuff for putting things back into li_vn_mode
 */
#define	PKT_LI_VN_MODE(li, vn, md) \
	((u_char)((((li) << 6) & 0xc0) | (((vn) << 3) & 0x38) | ((md) & 0x7)))

#define   MODE_CONTROL    6       /* control mode packet */

  /* in ntp_control.h */
#define	CTL_OP_READVAR	2
#define CTL_OP_MASK     0x1f
#define CTL_HEADER_LEN	12

	  /* Definitions specific to  snips */
#define POLLINTERVAL    (time_t)60  	/* interval between queries */
#define VARNM		"ntp"  		/* for EVENT.var.name field */
#define VARUNITS	"Stratum"	/* Units name */
#define TIMEOUT 	5		/* Seconds to wait for a reply */
#define TRIES		3		/* number of retries for udp pkt */

int ntpmon(char *host);   /* In dotted decimal addr */

#endif	/* ntpmon_h */

