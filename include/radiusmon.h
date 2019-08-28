/* $Header: /home/cvsroot/snips/include/radiusmon.h,v 1.2 2008/04/25 23:31:50 tvroon Exp $ */

#ifndef __radiusmon_h
#define __radiusmon_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#if defined(AIX) || defined(_AIX)
# include <sys/select.h>
#endif

/*
 *************** Definitions specific to  snips
 */
#define POLLINTERVAL    (time_t)60  	/* interval between queries */
#define VARNM		"radius"  	/* for EVENT.var.name field */
#define VARUNITS	"Status"	/* Units name */
#define TIMEOUT 	5		/* Seconds to wait for a reply */
#define RETRIES		2		/* since its UDP */

/*
 * If you want to ignore deny responses from the radius server and treat
 * them as 'valid' (since that means that the server is up anyway), then
 * define IGNORE_AUTHCODE
 */
#undef IGNORE_AUTHCODE			/* define to ignore rejects */

/*
 ******************* definitions specific to RADIUS
 */
#if defined(__alpha) && defined(__osf__)
typedef unsigned int UINT4;
#else
typedef unsigned long int UINT4;
#endif

/* move this to the os.h file ? */
#if defined(bsdi) || defined(__bsdi__)
#include        <machine/inline.h>
#include        <machine/endian.h>
#else   /* bsdi */
/* #include        <malloc.h>	/* */
/* extern char * sys_errlist[];	/* */
#endif  /* bsdi */

#define AUTH_HDR_LEN	20
#define AUTH_PW_LEN	16
#define	AUTH_VECTOR_LEN	16

typedef struct pw_auth_hdr {
  u_char	code;
  u_char	id;
  u_short	length;
  u_char	vector[AUTH_VECTOR_LEN];
  u_char	data[2];
} AUTH_HDR;

/* radius Attributes used in this code */
#define PW_USER_NAME	1
#define PW_PASSWORD	2
#define PW_NAS_ID	4
#define PW_NAS_PORT_ID	5
#define PW_SESSION_ID	44
#define NAS_PORT_TYPE	61

/* Packet types */
#define PW_AUTHENTICATION_REQUEST	1
#define PW_AUTHENTICATION_ACK		2
#define PW_AUTHENTICATION_REJECT	3

#ifndef	RADIUS_PORT
# define RADIUS_PORT	1645	/* destination port */
#endif
#define SOURCE_PORT	5678	/* dummy originating port number */

int radiusmon(char *host,int port, char *secret, char *user, char *pass, int timeout, int retries, int porttype);

#endif	/* radiusmon */

