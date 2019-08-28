/* $Header: /home/cvsroot/snips/trapmon/snips_specific.c,v 1.1 2008/04/25 23:31:52 tvroon Exp $ */

/*
 * Snips specific trapmon code.
 *
 * AUTHOR:
 *	Vikas Aggarwal, vikas@navya.com
 *
 */

/*
 * $Log: snips_specific.c,v $
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/09 04:04:18  vikas
 * For SNIPS v1.0
 *
 */

#ifdef SNIPS

#define _MAIN_
#include	"snips.h"
#include	"snips_funcs.h"
#include	"event_utils.h"
#include	"eventlog.h"
#undef _MAIN_
#include	"trapmon.h"
#include	"trapmon-local.h"
#include	"snmp/snmp.h"		/* part of CMU snmp code */

static char	*datafile, *configfile;		/* snips specific */

/*
 * Call snips startup, and open various files.
 */
int init_snips()
{
  int fd;

  prognm = "trapmon";			/* Program name */
  configfile = (char *)get_configfile();
  datafile = (char *)get_datafile();
  snips_startup();

  if (debug) 
  {
    fprintf(stderr, 
	    "prognm set to %s, datafile to %s.\n", prognm, datafile);
    fflush(stderr);
  }

  unlink(datafile);
  fd = open_datafile(datafile, O_WRONLY|O_CREAT|O_TRUNC);
  if (fd < 0) {
    closeeventlog();

    fprintf(stderr, "open() failed on %s\n", datafile);
    perror(prognm);
    exit(1);
  }
  close(fd);				/* just create the file */
  openeventlog();			/* Logging daemon */

  return 1;
}	/* init_snips() */

/*
 * Appends a new event to the end of the datafile. For an enterprise specific
 * trap, it adds the 'specific_type' of the trap for identification.
 */
int add_snips_event(addr, name, pdu)
    char *addr;					/* Address of trap sender */
    char *name;					/* Name of trap sender */
    struct snmp_pdu *pdu;			/* trap PDU */
{
    int fd;
    time_t clock;
    EVENT v;

    init_event(&v);
    clock = time(NULL);

    /*
     * In the following 'strncpy's, the null character at the end is
     * auto appended since to the bzero of the event struct.
     */
    strncpy(v.sender, prognm, sizeof(v.sender) - 1);	/* Set sender */
    strncpy(v.device.addr, addr, sizeof(v.device.addr) -1);	/* From address */
    strncpy(v.device.name, name, sizeof(v.device.name) -1);
    strncpy (v.var.units, VARUNITS, sizeof (v.var.units) - 1);

    if (pdu->command == TRP_REQ_MSG)	/* V1 trap */
    {
      int trap = pdu->trap_type;			/* Trap number */
      strncpy(v.var.name, trap_desc[trap].tname, sizeof(v.var.name) - 1);
      if (trap == SNMP_TRAP_ENTERPRISESPECIFIC)	/* specific type */
      { 
	static char spdutype[16] ;		/* convert int to string */
	sprintf (spdutype, "/%d\0", pdu->specific_type) ;
	if ( (sizeof(v.var.name) - strlen(v.var.name)) > 5)
	  strncpy (v.var.name + strlen(v.var.name), spdutype, 4) ;
	else
	  strncpy (v.var.name + sizeof(v.var.name) - 5, spdutype, 4) ;
      }

      v.severity = trap_desc[trap].tseverity;	/* Set the severity level */
      v.state = SETF_UPDOUN (v.state, trap_desc[trap].state);
      v.loglevel = trap_desc[trap].loglevel ;	/* logging level */
    }
    else if (pdu->command == SNMP_PDU_V2TRAP)		/* V2 trap */
    {
      struct variable_list *vars;
      char varbuf[4096];

      varbuf[0] = '\0';
      for (vars = pdu->variables; vars; vars = vars->next_variable)
	sprint_variable(varbuf, vars->name, vars->name_length, vars);

      strncpy(v.var.name, varbuf, sizeof(v.var.name) - 1);
      v.severity = E_ERROR;	/* Set a default severity level for all v2 */
      v.state = SETF_UPDOUN (v.state, n_DOWN);
      v.loglevel = E_ERROR ;	/* default logging level for all v2*/
      
    }	/* end if v1/v2 trap */
    else 
    {
      fprintf(stderr, "Unknown trap PDU type %d\n", pdu->command);
      return 0;
    }

    eventlog(&v);				/* Log the event */

    fd = open(datafile, O_WRONLY | O_APPEND);	/* Add the new EVENT */
    if (write_event(fd, &v) != sizeof(v)) {
	fprintf(stderr, "write() failed on %s during addevent()\n", datafile);
	closeeventlog();
	exit(1);
    }
    close(fd);

    /* Store the time after which the trap event is to be deleted */
    expire_at[numtraps++] = clock + TIME_TO_LIVE;  /* When to expire */

    if (debug)
	fprintf(stderr, "Added: devicenm=%s deviceaddr=%s varnm=%s severity=%d expire_at=%d\n",
		name, addr, v.var.name, v.severity, expire_at[numtraps - 1]);
    return(0);
}



/*+
 * Open the datafile twice - readonly and writeonly, which gives us two
 * independent file pointers for the datafile. Then:
 *
 *	(1) Read an EVENT and advance the read-pointer.
 *	(2) If this EVENT must be expired, go to step 4.
 *	(3) Otherwise, write the EVENT and advance the write-pointer.
 *	(4) If end-of-file not reached yet, go back to 1.
 *	(5) Truncate the file.
 *
 * If, when reading an EVENT, we discover it needs to be expired, then 
 * dont write it out i.e. only the read-pointer advances (and the
 * write-pointer remains at the same location).
 * After the whole file has been read and all the timed-out EVENTs have
 * been expired, the file must be truncated; it must be shortened by an
 * amount proportional to the number of EVENTs expired.
 *
 */
int expire_snips_events()
{
  int rd;				/* Read-only pointer */
  int wr;				/* Write-only pointer */
  int i;				/* Read index into expire_at[] */
  int j;				/* Write index into expire_at[] */
  EVENT v;				/* EVENT buffer */
  time_t expire_date;			/* expire_at[] buffer */
  time_t clock;				/* Current time */

  i = j = 0;
  clock = time(NULL);				/* Get the current time */
  if ((rd = open_datafile(datafile, O_RDONLY)) < 0)	/* Init read pointer */
  {
    fprintf(stderr, "%s (expire_snips_events): %s ", prognm, datafile) ;
    perror ("read open()");
    closeeventlog();
    return (-1);
  }
  /* Init write pointer */
  if ((wr = open_datafile(datafile, O_WRONLY)) < 0)
  {
    fprintf(stderr, "%s (expire_snips_events): %s ", prognm, datafile) ;
    perror ("write open()");
    closeeventlog();
    return (-1);
  }	

  if (debug)
    fprintf(stderr, "Entered expire_snips_events() numtraps=%d clock=%d... ",
	   numtraps, clock);

  while (read_event(rd, &v) == sizeof(v))/* Read EVENTs one by one */
    if ((expire_date = expire_at[i++]) > clock) {  /* expire this EVENT? */
      write_event(wr, &v);		/* No - keep this EVENT */
      expire_at[j++] = expire_date;
      if (debug)
	fprintf(stderr, "kept, ");
    }
    else if (debug)			/* Yes - expire this EVENT */
      fprintf(stderr, "expired, ");

  numtraps = j;
  ftruncate(wr, (off_t) j * sizeof(EVENT));	/* Shorten the file */

  if (debug)
    fprintf(stderr, "truncated, ");

  close(rd);					/* Done with read pointer */
  close(wr);					/* Done with write pointer */

  if (debug) {
    fprintf(stderr, "Done.\n");
    fflush(stderr);
  }
  return(0);
}	/* expire_snips_events() */

int help()
{
  (void) Usage();
}

#endif	/* SNIPS */
