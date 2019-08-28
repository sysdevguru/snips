/*
 * snmpdelta.c - Monitor deltas of integer valued SNMP variables
 *
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

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif /* HAVE_MALLOC_H */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include <snmp/snmp.h>

static char rcsid[] = 
"$Id: snmpdelta.c,v 1.10 1998/08/06 01:23:06 ryan Exp $";



#define MAX_ARGS 256
    
extern int  errno;

char *SumFile = "Sum";
char *Argv[MAX_ARGS];
int Argc;

/* Information about the handled variables */
struct varInfo {
  char *name;
  oid *oid;
  int oidlen;
  char descriptor[64];
  u_int value;
  int spoiled;

  /* Peak information */
  int    PeakCount;
  float  Peak;
  float  PeakAverage;

  /* Max value */
  float  Max;

  /* System Time */
  time_t Time;

};

/**********************************************************************
 *
 **********************************************************************/

static int Verbose = 0;

/**********************************************************************
 *
 * Interval information.
 *
 **********************************************************************/

static int Period = 1;

void period_SetPeriod(int P)
{
  if (Verbose)
    printf(">> Period set to %d seconds.\n", P);

  Period = P;
}

/* Wait for the next packet, or sleep.
 */
void period_WaitForPeriod(void)
{
  struct timeval time, *tv = &time;
  struct tm tm;
  int count;
  time_t nexthour;
  static int target = 0;

  gettimeofday(tv, (struct timezone *)0);
    
  if (target) {
    /* Already calculated the target time.  Just increase it's timer,
     * and wait for it.
     */
    target += Period;
  } else {

    /* Based on the current time */
    memcpy(&tm, localtime(&tv->tv_sec), sizeof(tm));

    /* Calculate the next hour */
    tm.tm_sec = 0;
    tm.tm_min = 0;
    tm.tm_hour++;

    /* And figure out the number of seconds until then */
    nexthour = mktime(&tm);

    /* Given the number of seconds remaining until the next hour,
     * calculate how many we must skew base time, so that interval
     * always ends on an hourly basis.
     */
    target = (nexthour - tv->tv_sec) % Period;
    if (target == 0)
      target = Period;
    target += tv->tv_sec;
  }

  /* ASSERT:  Target is the time_t value of the next time we should
   * wake up.
   */
    
  /* How much longer is it really?
   */
  tv->tv_sec = target - tv->tv_sec;
  if (tv->tv_usec != 0){
    tv->tv_sec--;
    tv->tv_usec = 1000000 - tv->tv_usec;
  }
  if (tv->tv_sec < 0){
    /* ran out of time, schedule immediately */
    return;
  }

  if (Verbose)
    printf(">> Sleeping for %ld seconds\n", tv->tv_sec);

  count = 1;
  while(count != 0){
    count = select(0, 0, 0, 0, tv);
    switch (count){
    case 0:
      break;
    case -1:
      if (errno == EINTR){
	printf("signal!\n");
	break;
      }
      /* FALLTHRU */
    default:
      perror("select");
      break;
    }
  }
}

/**********************************************************************
 *
 **********************************************************************/

static int PeakInterval = 0;

void peak_SetInterval(int P)
{
  if (Verbose)
    printf(">> Peak period set to %d seconds.\n", P);

  PeakInterval = P;
}
  

int peak_WaitForStart()
{
  struct timeval time, *tv = &time;
  struct tm tm;
  time_t SecondsAtNextHour;
  int target = 0;
  int seconds;

  seconds = Period * PeakInterval;

  /* Find the current time */
  gettimeofday(tv, (struct timezone *)0);

  /* Create a tm struct from it */
  memcpy(&tm, localtime(&tv->tv_sec), sizeof(tm));

  /* Calculate the next hour */
  tm.tm_sec = 0;
  tm.tm_min = 0;
  tm.tm_hour++;
  SecondsAtNextHour = mktime(&tm);
    
  /* Now figure out the amount of time to sleep */
  target = (SecondsAtNextHour - tv->tv_sec) % seconds;

  return(target);
}

/**********************************************************************
 *
 **********************************************************************/

void log(char *file, char *message)
{
  FILE *fp;

  fp = fopen(file, "a");
  if (fp == NULL){
    fprintf(stderr, "Couldn't open %s\n", file);
    return;
  }
  fprintf(fp, "%s\n", message);
  fclose(fp);
}

void sprint_descriptor(char *buffer, struct varInfo *vip)
{
  char buf[256], *cp;

  sprint_objid(buf, vip->oid, vip->oidlen);
  for(cp = buf; *cp; cp++)
    ;
  while(cp >= buf){
    if (isalpha(*cp))
      break;
    cp--;
  }
  while(cp >= buf){
    if (*cp == '.')
      break;
    cp--;
  }
  cp++;
  if (cp < buf)
    cp = buf;
  strcpy(buffer, cp);
}

void processFileArgs(char *fileName)
{
  FILE *fp;
  char buf[257], *cp;
  int blank, linenumber = 0;

  fp = fopen(fileName, "r");
  if (fp == NULL)
    return;
  while (fgets(buf, 256, fp)){
    linenumber++;
    if (strlen(buf) > 256){
      fprintf(stderr, "Line too long on line %d of %s\n",
	      linenumber, fileName);
      exit(1);
    }
    if (buf[0] == '#')
      continue;
    blank = TRUE;
    for(cp = buf; *cp; cp++)
      if (!isspace(*cp)){
	blank = FALSE;
	break;
      }
    if (blank)
      continue;
    Argv[Argc] = (char *)malloc(strlen(buf)); /* ignore newline */
    buf[strlen(buf) - 1] = 0;
    strcpy(Argv[Argc++], buf);
  }
  fclose(fp);
  return;
}

oid sysUpTimeOid[9] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
int sysUpTimeLen = 9;

void main(int argc, char **argv)
{
  struct snmp_session session, *ss;
  struct snmp_pdu *pdu, *response;
  struct variable_list *vars;
  int	arg;
  char *gateway = NULL;
  char *community = NULL;

  int	count, current_name = 0;
  struct varInfo varinfo[128], *vip;
  u_int value;
  float printvalue;
  int fileout = 0, dosum = 0;
  int keepSeconds = 0;
  int sum;
  char filename[128];
  struct timeval tv;
  struct tm tm;
  char timestring[64], valueStr[64], peakStr[64];
  char outstr[256];
  int status;
  int begin, end, last_end;
  int varbindsPerPacket = 60;
  int print = 1;

  char buf[64];

  int Port    = SNMP_DEFAULT_REMPORT;
  int Retries = SNMP_DEFAULT_RETRIES;
  int Timeout = SNMP_DEFAULT_TIMEOUT;

  int PrintTimeStamp = 0;
  int PrintMax = 0;

  int DeltaTime = 0;
  time_t Time_Last = 0; /* Last time we checked */
  time_t Time_Now;      /* Current time */
  time_t Time_Delta;    /* Time since last check */
    
  init_mib();

  /*
   * usage: snmpdelta gateway-name community-name [-f commandFile] [-l]
   * [-v vars/pkt] [-s] [-t] [-S] [-p period] [-P peaks] [-k] [ -Z port ]
   * -V [-L SumFileName] variable list ..
   */

  for(arg = 1; arg < argc && arg < MAX_ARGS; arg++){
    Argv[arg] = (char *)malloc(strlen(argv[arg]) + 1);
    strcpy(Argv[arg], argv[arg]);
  }
  Argc = argc;

  for(arg = 1; arg < Argc; arg++){
    if (Argv[arg][0] == '-'){
      switch(Argv[arg][1]){
      case 'Z':
	Port = atoi(Argv[++arg]);
	break;
      case 'R':
	Retries = atoi(argv[++arg]);
	break;
      case 'T':
	Timeout = atoi(argv[++arg]);
	break;

      case 'V':
	Verbose = 1;
	break;
      case 'd':
	if (Verbose)
	  printf(">> Will dump packets\n");
	snmp_dump_packet(1);
	break;
      case 'p':
	period_SetPeriod(atoi(Argv[++arg]));
	break;
      case 'P':
	peak_SetInterval(atoi(Argv[++arg]));
	break;
      case 't':
	DeltaTime = 1;
	break;
      case 's':
	PrintTimeStamp = 1;
	break;
      case 'm':
	PrintMax = 1;
	break;


      case 'v':
	varbindsPerPacket = atoi(Argv[++arg]);
	break;
      case 'S':
	dosum = 1;
	break;
      case 'f':
	processFileArgs(Argv[++arg]);
	break;
      case 'l':
	fileout = 1;
	break;
      case 'L':
	SumFile = Argv[++arg];
	break;
      case 'k':
	keepSeconds = 1;
	break;

      default:
	printf("invalid option: -%c\n", Argv[arg][1]);
	break;
      }
      continue;
    }
    if (gateway == NULL){
      gateway = Argv[arg];
    } else if (community == NULL){
      community = Argv[arg]; 
    } else {
      varinfo[current_name++].name = Argv[arg];
    }
  }
    
  if (!(gateway && community && current_name > 0)){
    fprintf(stderr, "usage: snmpdelta gateway-name community-name [-f commandFile]\n[-l] [-L SumFileName] [-s] [-k] [-t] [-S] [-v vars/pkt]\n [-p period] [-P peaks] [-Z port] [-V] oid [oid ...]\n");
    exit(1);
  }
    
  if (dosum) {
    varinfo[current_name++].name = 0;
  }

  memset((char *)&session, '\0', sizeof(struct snmp_session));
  session.peername = gateway;
  session.community = (u_char *)community;
  session.community_len = strlen((char *)community);
  session.retries     = Retries;
  session.timeout     = Timeout;
  session.remote_port = Port;
  snmp_synch_setup(&session);
  ss = snmp_open(&session);
  if (ss == NULL){
    fprintf(stderr, "Couldn't open snmp\n");
    exit(-1);
  }
    
  for(count = 0; count < current_name; count++){
    vip = varinfo + count;
    if (vip->name){
      vip->oidlen = MAX_NAME_LEN;
      vip->oid = (oid *)malloc(sizeof(oid) * vip->oidlen);
      if (!read_objid(vip->name, vip->oid, &vip->oidlen)) {
	fprintf(stderr, "Invalid object identifier: %s\n", vip->name);
      }
      sprint_descriptor(vip->descriptor, vip);
    } else {
      vip->oidlen = 0;
      strcpy(vip->descriptor, SumFile);
    }
    vip->value = 0;
    vip->Time  = 0;
    vip->Max   = 0;
    if (PeakInterval) {
      vip->PeakCount = -1;
      vip->Peak = 0;
      vip->PeakAverage = 0;
    }
  }
    
  /* Wait until we should start */
  period_WaitForPeriod();

  end = current_name;
  sum = 0;
  while(1){
    pdu = snmp_pdu_create(SNMP_PDU_GET);
	
    /* Ask about time only if we care */
    if (DeltaTime)
      snmp_add_null_var(pdu, sysUpTimeOid, sysUpTimeLen);

    if (end == current_name)
      count = 0;
    else
      count = end;
    begin = count;
    for(; count < current_name
	  && count < begin + varbindsPerPacket - DeltaTime; count++){
      if (varinfo[count].oidlen)
	snmp_add_null_var(pdu, varinfo[count].oid,
			  varinfo[count].oidlen);
    }
    last_end = end;
    end = count;
	
  retry:
    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_SUCCESS){
      if (response->errstat == SNMP_ERR_NOERROR){

	vars = response->variables;

	/* Print a timestamp
	 */
	if (PrintTimeStamp) {

	  gettimeofday(&tv, (struct timezone *)0);
	  memcpy(&tm, localtime(&tv.tv_sec), sizeof(tm));

	  /* Print seconds only if we're NOT keeping track of peaks
	   * or told to.
	   */
	  if (((Period % 60) && (!PeakInterval || 
				 ((Period * PeakInterval) % 60)))
	      || keepSeconds)
	    sprintf(timestring, " [%02d:%02d:%02d %d/%d]",
		    tm.tm_hour, tm.tm_min, tm.tm_sec,
		    tm.tm_mon+1, tm.tm_mday);
	  else
	    sprintf(timestring, " [%02d:%02d %d/%d]",
		    tm.tm_hour, tm.tm_min,
		    tm.tm_mon+1, tm.tm_mday);
	}

	/* Time
	 */
	if (DeltaTime) {
	  if (!vars) {
	    printf("Missing variable in reply\n");
	    continue;
	  } else {
	    Time_Now = *(vars->val.integer);
	  }
	  vars = vars->next_variable;
	} else {
	  Time_Now = 1;
	}

	/* Now, parse the variable information.
	 */
 	for(count = begin; count < end; count++) {
	  vip = varinfo + count;

	  if (vip->oidlen) {
	    if (!vars){
	      printf("Missing variable in reply\n");
	      break;
	    }
	    value      = *(vars->val.integer) - vip->value;
	    vip->value = *(vars->val.integer);
	    vars       = vars->next_variable;
	  } else {
	    value      = sum;
	    sum        = 0;
	  }

	  /* Calculate difference in time since last poll
	   */
	  Time_Delta = (Time_Now - vip->Time);
	  if (Time_Delta <= 0)
	    Time_Delta = 100; /* Default */

	  /* Update time stat
	   */
	  Time_Last = vip->Time;
	  vip->Time = Time_Now;

	  /* Only print stats if we have checked at least once before.
	   */
	  if (Time_Last == 0)
	    continue;

	  if (vip->oidlen)
	    sum += value;

	  sprintf(outstr, "%s %s", timestring, vip->descriptor);

	  /* Print time interval
	   */
	  if (DeltaTime) {
	    /* per second average */
	    printvalue = ((float)value * 100) / Time_Delta;
	    sprintf(valueStr, " /sec: %.2f", printvalue);
	  } else {
	    /* per seconds passed */
	    printvalue = value;
	    sprintf(valueStr, " /%d sec: %.0f",
		    Period, printvalue);
	  }

	  if (!PeakInterval) {
	    strcat(outstr, valueStr);
	  } else {
	    print = 0;
	    if (vip->PeakCount == -1){
	      if (peak_WaitForStart() == 0)
		vip->PeakCount = 0;
	    } else {
	      vip->PeakAverage += printvalue;
	      if (vip->Peak < printvalue)
		vip->Peak = printvalue;
	      if (++vip->PeakCount == PeakInterval){


		if (DeltaTime)
		  /* Print the per-second peak */
		  sprintf(peakStr,
			  " /sec: %.2f	(%d sec Peak: %.2f)",
			  vip->PeakAverage/vip->PeakCount,
			  Period, vip->Peak);
		else
		  /* Print the total peak */
		  sprintf(peakStr,
			  " /%d sec: %.0f	(%d sec Peak: %.0f)",
			  Period,
			  vip->PeakAverage/vip->PeakCount,
			  Period, vip->Peak);
		
		vip->PeakAverage = 0;
		vip->Peak = 0;
		vip->PeakCount = 0;
		print = 1;
		strcat(outstr, peakStr);
	      }
	    }
	  }

	  /* Print the max value
	   */
	  if (PrintMax){
	    if (printvalue > vip->Max){
	      vip->Max = printvalue;
	    }
	    if (DeltaTime)
	      sprintf(buf, "\t(Max: %.2f)", vip->Max);
	    else
	      sprintf(buf, "\t(Max: %.0f)", vip->Max);
	    strcat(outstr, buf);
	  }



	  if (print){
	    if (fileout){
	      sprintf(filename, "%s-%s", gateway, vip->descriptor);
	      log(filename, outstr + 1);
	    } else {
	      printf("%s\n", outstr + 1);
	      fflush(stdout);
	    }
	  }
	}
      } else {
	if (response->errstat == SNMP_ERR_TOOBIG){
	  if (response->errindex <= varbindsPerPacket
	      && response->errindex > 0){
	    varbindsPerPacket = response->errindex - 1;
	  } else {
	    if (varbindsPerPacket > 30)
	      varbindsPerPacket -= 5;
	    else
	      varbindsPerPacket--;
	  }
	  if (varbindsPerPacket <= 0)
	    exit(5);
	  end = last_end;
	  continue;
	} else if (response->errstat == SNMP_ERR_NOSUCHNAME){
	  printf("This object doesn't exist on %s: ", gateway);
	  for(count = 1, vars = response->variables;
	      vars && count != response->errindex;
	      vars = vars->next_variable, count++)
	    ;
	  if (vars)
	    print_oid_nums(vars->name, vars->name_length);
	  printf("\n");
	} else {
	  printf("Error in packet.\nReason: %s\n",
		 snmp_errstring(response->errstat));
	}
	exit(1);
	if ((pdu = snmp_fix_pdu(response, SNMP_PDU_GET)) != NULL)
	  goto retry;
      }
	    
    } else if (status == STAT_TIMEOUT){
      printf("No Response from %s\n", gateway);
      response = 0;
    } else {    /* status == STAT_ERROR */
      printf("An error occurred, Quitting\n");
      response = 0;
      break;
    }
	
    if (response)
      snmp_free_pdu(response);
    if (end == current_name){
      period_WaitForPeriod();
    }
  }
  snmp_close(ss);
}

