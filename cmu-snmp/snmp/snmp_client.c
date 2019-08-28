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
 * $Id: snmp_client.c,v 1.15 1998/12/06 16:38:30 ryan Exp $
 * 
 **********************************************************************/

/* Our autoconf variables */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>

#ifdef WIN32
#include <winsock2.h>
#else /* WIN32 */
#include <sys/param.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif /* WIN32 */

#ifdef HAVE_STRINGS_H
# include <strings.h>
#else /* HAVE_STRINGS_H */
# include <string.h>
#endif /* HAVE_STRINGS_H */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif /* HAVE_BSTRING_H */

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

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

static char rcsid[] =
"$Id";

#include "asn1.h"
#include "snmp_error.h"
#include "snmp_pdu.h"
#include "snmp_vars.h"

#include "snmp_session.h"
#include "snmp_api.h"
#include "snmp_client.h"
#include "snmp_api_error.h"

/* Define these here, as they aren't defined normall under
 * cygnus Win32 stuff.
 */
#undef timerclear
#define timerclear(tvp) (tvp)->tv_sec = (tvp)->tv_usec = 0

/* #define DEBUG_CLIENT 1 */

extern int snmp_errno;

int snmp_synch_input(int Operation, 
		     struct snmp_session *Session, 
		     int RequestID, 
		     struct snmp_pdu *pdu,
		     void *magic)
{
  struct variable_list *var;
  struct synch_state *state = (struct synch_state *)magic;
  struct snmp_pdu *newpdu;

  struct variable_list *varPtr;

#ifdef DEBUG_CLIENT
  printf("CLIENT %x: Synchronizing input.\n", (unsigned int)pdu);
#endif

  /* Make sure this is the proper request
   */
  if (RequestID != state->reqid)
    return 0;
  state->waiting = 0;

  /* Did we receive a Get Response
   */
  if (Operation == RECEIVED_MESSAGE && pdu->command == SNMP_PDU_RESPONSE) {

    /* clone the pdu */
    state->pdu = newpdu = snmp_pdu_clone(pdu);
    newpdu->variables = 0;

    /* Clone all variables */
    var = pdu->variables;
    if (var != NULL) {
      newpdu->variables = snmp_var_clone(var);

      varPtr = newpdu->variables;

      /* While there are more variables */
      while(var->next_variable) {

	/* Clone the next one */
	varPtr->next_variable = snmp_var_clone(var->next_variable);

	/* And move on */
	var    = var->next_variable;
	varPtr = varPtr->next_variable;

      }
      varPtr->next_variable = NULL;
    }
    state->status = STAT_SUCCESS;
    snmp_errno = 0;  /* XX all OK when msg received ? */
    Session->s_snmp_errno = 0;
  } else if (Operation == TIMED_OUT) {
    state->pdu = NULL; /* XX from other lib ? */
    state->status = STAT_TIMEOUT;
    snmp_errno = SNMPERR_NO_RESPONSE;
    Session->s_snmp_errno = SNMPERR_NO_RESPONSE;
  }
  return 1;
}

int snmp_synch_response(struct snmp_session *Session, 
			struct snmp_pdu *PDU,
			struct snmp_pdu **ResponsePDUP)
{
  struct synch_state *state = Session->snmp_synch_state;
  int numfds, count;
  fd_set fdset;
  struct timeval timeout, *tvp;
  int block;

  state->reqid = snmp_send(Session, PDU);
  if (state->reqid == 0) {
    *ResponsePDUP = NULL;
    snmp_free_pdu(PDU);
    return STAT_ERROR;
  }
  state->waiting = 1;

  while(state->waiting) {
    numfds = 0;
    FD_ZERO(&fdset);
    block = 1;
    tvp = &timeout;
    timerclear(tvp);
    snmp_select_info(&numfds, &fdset, tvp, &block);
    if (block == 1)
      tvp = NULL;	/* block without timeout */
    count = select(numfds, &fdset, 0, 0, tvp);
    if (count > 0){
      snmp_read(&fdset);
    } else switch(count){
    case 0:
      snmp_timeout();
      break;
    case -1:
      if (errno == EINTR){
	continue;
      } else {
	snmp_errno = SNMPERR_GENERR;
	/* CAUTION! if another thread closed the socket(s)
	   waited on here, the session structure was freed.
	   It would be nice, but we can't rely on the pointer.
	    Session->s_snmp_errno = SNMPERR_GENERR;
	    Session->s_errno = errno;
	 */
      }
      /* FALLTHRU */
    default:
      snmp_free_pdu(PDU); /* XX other lib ? */
      *ResponsePDUP = NULL; /* XX other lib ? */
      return STAT_ERROR;
    }
  }
  *ResponsePDUP = state->pdu;
  return state->status;
}

int snmp_sess_synch_response(void * sessp,
			struct snmp_pdu *PDU,
			struct snmp_pdu **ResponsePDUP)
{
  struct snmp_session *Session;
  struct synch_state *state;
  int numfds, count;
  fd_set fdset;
  struct timeval timeout, *tvp;
  int block;

  Session = snmp_sess_session(sessp);
  state = Session->snmp_synch_state;

  state->reqid = snmp_sess_send(sessp, PDU);
  if (state->reqid == 0) {
    *ResponsePDUP = NULL;
    snmp_free_pdu(PDU);
    return STAT_ERROR;
  }
  state->waiting = 1;

  while(state->waiting) {
    numfds = 0;
    FD_ZERO(&fdset);
    block = 1;
    tvp = &timeout;
    timerclear(tvp);
    snmp_sess_select_info(sessp, &numfds, &fdset, tvp, &block);
    if (block == 1)
      tvp = NULL;	/* block without timeout */
    count = select(numfds, &fdset, 0, 0, tvp);
    if (count > 0){
      snmp_sess_read(sessp, &fdset);
    } else switch(count){
    case 0:
      snmp_sess_timeout(sessp);
      break;
    case -1:
      if (errno == EINTR){
	continue;
      } else {
	snmp_errno = SNMPERR_GENERR;
	/* CAUTION! if another thread closed the socket(s)
	   waited on here, the session structure was freed.
	   It would be nice, but we can't rely on the pointer.
	    Session->s_snmp_errno = SNMPERR_GENERR;
	    Session->s_errno = errno;
	 */
      }
      /* FALLTHRU */
    default:
      snmp_free_pdu(PDU); /* XX other lib ? */
      *ResponsePDUP = NULL; /* XX other lib ? */
      return STAT_ERROR;
    }
  }
  *ResponsePDUP = state->pdu;
  return state->status;
}

void snmp_synch_reset(struct snmp_session *Session)
{
  if (Session && Session->snmp_synch_state)
     free(Session->snmp_synch_state);
}

void snmp_synch_setup(struct snmp_session *Session)
{
  struct synch_state *rp = (struct synch_state *)malloc(sizeof(struct synch_state));

  rp->waiting = 0;
  rp->pdu = NULL;
  Session->snmp_synch_state = rp;

  Session->callback = snmp_synch_input;
  Session->callback_magic = (void *)rp;
}
