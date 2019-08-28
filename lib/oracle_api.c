/* $Header: /home/cvsroot/snips/lib/oracle_api.c,v 0.1 2001/07/08 22:22:41 vikas Exp $ */

/*
 * THIS IS INCOMPLETE WORK.
 */

/*
 * Oracle interface to SNIPS
 *
 *	snips_open_db()
 *	snips_close_db()
 *	snips_insert_dbevent()
 *	snips_update_dbevent()
 *	snips_read_dbevent()
 *
 * To compile:
 *    make -f demo_rdbms.mk build EXE=oracle_api OBJS="oracle_api.o"
 *    make -f demo_rdbms.mk build_static EXE=oracle_api OBJS="oracle_api.o"
 *
 *	SETENV LD_LIBRARY_PATH /usr/local/oracle/lib
 *
 * AUTHOR:
 *	Vikas Aggarwal,  vikas@navya.com
 *
 */

/*
 * $Log: oracle_api.c,v $
 * Revision 0.1  2001/07/08 22:22:41  vikas
 * For SNIPS interface to databases. Incomplete work.
 *
 */

/* Copyright Netplex Technologies Inc, */


#include <oci.h>	/* Oracle include file */
#include "snips.h"	/* in ../include */

#define TEST		/* for main() */

/*------------------------ Global Variables -------------------------------*/

static	text *username = (text *)"snipsUser";
static	text *password = (text *)"snipsPasswd";
static	text *dbserver = (text *)"localhost";
/* the database name is snips_EVENTS  set in the sqlstmt below */


#define MAXBINDS	32	/* max fields in event structure */

static	boolean logged_on = FALSE;

static OCIEnv	*envh;
static OCIServer *srvh;
static OCISvcCtx *svch;
static OCISession *auth;
static OCIError *errh;

static sword init_handles(), attach_server(), log_on(), bind_input();
static void logout_detach_server(), free_handles(), report_error();

/*========================== API FUNCTIONS ======================*/

snips_open_db()
{
  if (logged_on == TRUE)
    return OCI_SUCCESS;

  /* Initialize the Environment and allocate handles */
  if (init_handles(&envh, &svch, &errh, &srvh, &auth)) {
    fprintf(stderr, "FAILED: init_handles()\n");
    snips_close_db();
    return OCI_ERROR;
  }

  /* Attach to the database server */
  if (attach_server((ub4) OCI_DEFAULT, srvh, errh, svch)) {
    fprintf(stderr, "FAILED: attach_server()\n");
    snips_close_db();
    return OCI_ERROR;
  }

  /* Logon to the server and begin a session */
  if (log_on(auth, errh, svch, username, password))
  {
    fprintf(stderr, "FAILED: log_on()\n");
    snips_close_db();
    return OCI_ERROR;
  }

  logged_on = TRUE;
  return (OCI_SUCCESS);
}

/*---------------------------------------------------------------------*/
/* Close connection to Oracle database and cleanup all pointers        */
/*---------------------------------------------------------------------*/
sword snips_close_db()
{
  if (logged_on)
    logout_detach_server(svch, srvh, errh, auth, username);

  free_handles(envh, svch, srvh, errh, auth);
  return OCI_SUCCESS;
}

snips_update_dbevent(pv)
  EVENT *pv;
{
  static OCIStmt *stmth;
  static OCIBind *bndh[MAXBINDS];
  int i;
  text *sqlstmt, *sqlstmt1;

  sqlstmt = (text *)
    "UPDATE snips_EVENTS SET VAR_VALUE = :var_value, ", \
    "VAR_THRESHOLD = :var_threshold, SEVERITY = :severity, ", \
    "LOGLEVEL = :loglevel, STATE = :state, RATING = :rating, " \
    "WHERE SENDER = :sender AND DEVICE_NAME = :device_name " \
    "AND DEVICE_SUBDEV = :device_subdev AND VAR_NAME = :var_name";

  /* this is an INSERT statement instead of an UPDATE */
  sqlstmt1 = (text *)
    "INSERT INTO snips_events (SENDER, ", \
    "DEVICE_NAME, DEVICE_ADDR, DEVICE_SUBDEV, ", \
    "VAR_NAME, VAR_VALUE, VAR_THRESHOLD, VAR_UNITS, ", \
    "SEVERITY, LOGLEVEL, STATE, RATING, EVENTTIME, POLLTIME) ", \
    "VALUES (:sender, :device_name, :device_addr, :device_subdev, ", \
    ":var_name, :var_value, :var_threshold, :var_units, ", \
    ":severity, :loglevel, :state, :rating, :eventtime, :polltime)";

  /* this needs to only be done  once */
  if (!stmth)
  {
    snips_open_db();
    if (!logged_on)
    {
      fprintf(stderr, "snips_write_dbevent() - Oracle error in login\n");
      return (-1);
    }
    for (i = 0; i < MAXBINDS; i++)
      bndh[i] = (OCIBind *) 0;
  
    /* Allocate a statement handle */
    if (OCIHandleAlloc((dvoid *)envh, (dvoid **) &stmth,
		       (ub4)OCI_HTYPE_STMT, (CONST size_t) 0, (dvoid **) 0))
    {
      fprintf(stderr, "FAILED: alloc statement handle\n");
      snips_close_db();
      return (-1);
    }
    /* Prepare the statement FIX FIX, does this need to be done each time? */
    if (OCIStmtPrepare(stmth, errh, sqlstmt, (ub4)strlen((char *)sqlstmt),
		       (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT))
    {
      fprintf(stderr,"FAILED: OCIStmtPrepare() update\n");
      report_error(errh);
      if (stmth)
	(void) OCIHandleFree((dvoid *) stmth, (ub4) OCI_HTYPE_STMT);
      return -1;
    }

    /* Bind all the input buffers */
    if (bind_input(stmth, bndh, errh)) {
      (void) OCIHandleFree((dvoid *) stmth, (ub4) OCI_HTYPE_STMT);
      return -1;
    }
  }	/* if !stmph */

  /* FIX FIX, check fields */
  if (OCIStmtExecute(svch, stmth, errh, (ub4) 1, (ub4) 0,
                    (CONST OCISnapshot*) 0, (OCISnapshot*) 0,
                    (ub4) OCI_DEFAULT | OCI_COMMIT_ON_SUCCESS))
  {
    fprintf(stderr, "FAILED: OCIStmtExecute() update\n");
    report_error(errh);
    (void) OCIHandleFree((dvoid *) stmth, (ub4) OCI_HTYPE_STMT);
    return -1;
  }

  /* Commit the changes */
/*  (void) OCITransCommit(svch, errh, (ub4) 0);	/* */

  return 0;	/* ok */
}	/* snips_write_dbevent() */



/*========================== UTILITY FUNCTIONS ======================*/

/* ----------------------------------------------------------------- */
/* Initialize environment, allocate handles                          */ 
/* ----------------------------------------------------------------- */
static sword init_handles(envhp, svchp, errhp, srvhp, authp)
  OCIEnv **envhp; 
  OCISvcCtx **svchp; 
  OCIError **errhp; 
  OCIServer **srvhp; 
  OCISession **authp; 
{
  fprintf (stderr,"Environment setup ....\n");

  /* Initialize the OCI Process */
  if (OCIEnvCreate(envhp, OCI_DEFAULT, (dvoid *)0, 
                    (dvoid * (*)(dvoid *, size_t)) 0,
                    (dvoid * (*)(dvoid *, dvoid *, size_t))0, 
                    (void (*)(dvoid *, dvoid *)) 0,
		    (size_t) 0, (dvoid **) 0 ))
  {
    fprintf(stderr,"FAILED: OCIEnvCreate()\n");
    return OCI_ERROR;
  }

  /* Allocate a service handle */
  if (OCIHandleAlloc((dvoid *) *envhp, (dvoid **) svchp,
                     (ub4) OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) 0))
  {
    fprintf(stderr,"FAILED: OCIHandleAlloc() on service handle\n");
    return OCI_ERROR;
  }
 
  /* Allocate an error handle */
  if (OCIHandleAlloc((dvoid *) *envhp, (dvoid **) errhp,
                     (ub4) OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) 0))
  {
    fprintf(stderr,"FAILED: OCIHandleAlloc() on error handle\n");
    return OCI_ERROR;
  }
 
  /* Allocate a server handle */
  if (OCIHandleAlloc((dvoid *) *envhp, (dvoid **) srvhp,
                     (ub4) OCI_HTYPE_SERVER, (size_t) 0, (dvoid **) 0))
  {
    fprintf(stderr,"FAILED: OCIHandleAlloc() on server handle\n");
    return OCI_ERROR;
  }

  /* Allocate a authentication handle */
  if (OCIHandleAlloc((dvoid *) *envhp, (dvoid **) authp,
                     (ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0))
  {
    fprintf(stderr,"FAILED: OCIHandleAlloc() on authentication handle\n");
    return OCI_ERROR;
  }

  return OCI_SUCCESS;
}

/* ----------------------------------------------------------------- */
/* Attach to server 'dbserver'.                                      */ 
/* ----------------------------------------------------------------- */
static sword attach_server(mode, srvhp, errhp, svchp)
  ub4 mode; 
  OCIServer *srvhp;
  OCIError *errhp; 
  OCISvcCtx *svchp;
{
  fprintf(stderr, "Attaching to %s\n", dbserver);
  if (OCIServerAttach(srvhp, errhp, (text *) dbserver,
                     (sb4) strlen((char *)dbserver), (ub4) OCI_DEFAULT))
  {
    fprintf(stderr,"FAILED: OCIServerAttach(%s)\n", (char *)dbserver);
    return OCI_ERROR;
  }

  /* Set the server handle in the service handle */
  if (OCIAttrSet((dvoid *) svchp, (ub4) OCI_HTYPE_SVCCTX,
                 (dvoid *) srvhp, (ub4) 0, (ub4) OCI_ATTR_SERVER, errhp))
  {
    fprintf(stderr,"FAILED: OCIAttrSet() server attribute\n");
    return OCI_ERROR;
  }

  return OCI_SUCCESS;
}
/* ----------------------------------------------------------------- */
/* Logon to the database using given username, password              */
/* ----------------------------------------------------------------- */
static sword log_on(authp, errhp, svchp, uid, pwd)
  OCISession *authp; 
  OCIError *errhp; 
  OCISvcCtx *svchp; 
  text *uid; 
  text *pwd; 
{
  /* Set attributes in the authentication handle */
  if (OCIAttrSet((dvoid *) authp, (ub4) OCI_HTYPE_SESSION,
                 (dvoid *) uid, (ub4) strlen((char *) uid),
                 (ub4) OCI_ATTR_USERNAME, errhp))
  {
    fprintf(stderr,"FAILED: OCIAttrSet() username\n");
    return OCI_ERROR;
  }
  if (OCIAttrSet((dvoid *) authp, (ub4) OCI_HTYPE_SESSION,
                 (dvoid *) pwd, (ub4) strlen((char *) pwd),
                 (ub4) OCI_ATTR_PASSWORD, errhp))
  {
    fprintf(stderr,"FAILED: OCIAttrSet() password\n");
    return OCI_ERROR;
  }

  fprintf(stderr,"Logging on as %s  ....\n", uid);

  if (OCISessionBegin(svchp, errhp, authp, OCI_CRED_RDBMS, OCI_DEFAULT))
  {
    fprintf(stderr,"FAILED: OCIAttrSet() password\n");
    return OCI_ERROR;
  }

  fprintf(stderr,"%s logged on.\n", uid);

  /* Set the authentication handle in the Service handle */
  if (OCIAttrSet((dvoid *) svchp, (ub4) OCI_HTYPE_SVCCTX,
                 (dvoid *) authp, (ub4) 0, (ub4) OCI_ATTR_SESSION, errhp))
  {
    fprintf(stderr,"FAILED: OCIAttrSet() session\n");
    return OCI_ERROR;
  }

  return OCI_SUCCESS;
}

/* ----------------------------------------------------------------- */
/*  Free the specified handles                                       */
/* ----------------------------------------------------------------- */
static void free_handles(envhp, svchp, srvhp, errhp, authp)
  OCIEnv *envhp; 
  OCISvcCtx *svchp; 
  OCIServer *srvhp; 
  OCIError *errhp; 
  OCISession *authp; 
{
  fprintf(stderr,"Freeing handles ...\n");
 
  if (srvhp)
    (void) OCIHandleFree((dvoid *) srvhp, (ub4) OCI_HTYPE_SERVER);
  if (svchp)
    (void) OCIHandleFree((dvoid *) svchp, (ub4) OCI_HTYPE_SVCCTX);
  if (errhp)
    (void) OCIHandleFree((dvoid *) errhp, (ub4) OCI_HTYPE_ERROR);
  if (authp)
    (void) OCIHandleFree((dvoid *) authp, (ub4) OCI_HTYPE_SESSION);
  if (envhp)
    (void) OCIHandleFree((dvoid *) envhp, (ub4) OCI_HTYPE_ENV);

  return;
}

/* ----------------------------------------------------------------- */
/* Print the error message                                           */
/* ----------------------------------------------------------------- */
static void report_error(errhp)
  OCIError *errhp;
{
  text  msgbuf[512];
  sb4   errcode = 0;
 
  (void) OCIErrorGet((dvoid *) errhp, (ub4) 1, (text *) NULL, &errcode,
                       msgbuf, (ub4) sizeof(msgbuf), (ub4) OCI_HTYPE_ERROR);
  fprintf(stderr,"ERROR CODE = %d\n", errcode);
  fprintf(stderr,"%.*s\n", 512, msgbuf);
  return;
}

/*-------------------------------------------------------------------*/ 
/* Logout and detach from the server                                 */
/*-------------------------------------------------------------------*/ 
static void logout_detach_server(svchp, srvhp, errhp, authp, username)
  OCISvcCtx *svchp; 
  OCIServer *srvhp; 
  OCIError *errhp;
  OCISession *authp; 
  text *username;
{
  if (OCISessionEnd(svchp, errhp, authp, (ub4) 0))
  {
    fprintf(stderr,"FAILED: OCISessionEnd()\n");
    report_error(errhp);
  }

  fprintf(stderr,"%s Logged off.\n", username);

  if (OCIServerDetach(srvhp, errhp, (ub4) OCI_DEFAULT))
  {
    fprintf(stderr,"FAILED: OCISessionEnd()\n");
    report_error(errhp);
  }
 
  fprintf(stderr,"Detached from server.\n");

  return;
}

/*===================== END OF UTILITY FUNCTIONS ======================*/


/* ===================== Local Functions ============================*/ 

static sword bind_input(pv, stmthp, bndhp, errhp) 
  EVENT *pv;
  OCIStmt *stmthp;
  OCIBind *bndhp[];
  OCIError *errhp;
{
  int i = 0;
  /* FIX FIX , what is OCI_DATA_AT_EXEC see pg 5-34 */
  if (
      OCIBindByName(stmthp, &bndhp[i++], errhp,
		    (text *) ":sender", strlen(":sender"),
		    (dvoid *)  &(pv->sender),
		    (sb4) sizeof(pv->sender), SQLT_STR,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":device_name", strlen(":device_name"),
		    (dvoid *)  &(pv->device.name),
		    (sb4) sizeof(pv->device.name), SQLT_STR,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":device_addr", strlen(":device_addr"),
		    (dvoid *)  &(pv->device.addr),
		    (sb4) sizeof(pv->device.addr), SQLT_STR,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":device_subdev", strlen(":device_subdev"),
		    (dvoid *)  &(pv->device.subdev),
		    (sb4) sizeof(pv->device.subdev), SQLT_STR,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":var_name", strlen(":var_name"),
		    (dvoid *)  &(pv->var.name),
		    (sb4) sizeof(pv->var.name), SQLT_STR,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":var_value", strlen(":var_value"),
		    (dvoid *)  &(pv->var.value),
		    (sb4) sizeof(pv->var.value),  SQLT_UIN,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":var_threshold", strlen(":var_threshold"),
		    (dvoid *)  &(pv->var.threshold),
		    (sb4) sizeof(pv->var.threshold),  SQLT_UIN,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":var_units", strlen(":var_units"),
		    (dvoid *)  &(pv->var.units),
		    (sb4) sizeof(pv->var.units), SQLT_STR,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":severity", strlen(":severity"),
		    (dvoid *)  &(pv->severity),
		    (sb4) sizeof(pv->severity), SQLT_INT,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":loglevel", strlen(":loglevel"),
		    (dvoid *)  &(pv->loglevel),
		    (sb4) sizeof(pv->loglevel), SQLT_INT,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":state", strlen(":state"),
		    (dvoid *)  &(pv->state),
		    (sb4) sizeof(pv->state), SQLT_INT,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":rating", strlen(":rating"),
		    (dvoid *)  &(pv->rating),
		    (sb4) sizeof(pv->rating), SQLT_INT,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":eventtime", strlen(":eventtime"),
		    (dvoid *)  &(pv->eventtime),
		    (sb4) sizeof(pv->eventtime), SQLT_UIN,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
   || OCIBindByName(stmthp, &bndhp[i++], errhp, 
		    (text *) ":polltime", strlen(":polltime"),
		    (dvoid *)  &(pv->polltime),
		    (sb4) sizeof(pv->polltime), SQLT_UIN,
		    (dvoid *) 0, (ub2 *)0, (ub2 *)0, (ub4) 0, (ub4 *) 0,
		    (ub4) OCI_DATA_AT_EXEC)
    )
  {
    fprintf(stderr, "FAILED: OCIBindByName()\n");
    report_error(errhp);
    return OCI_ERROR;
  }

  return OCI_SUCCESS;
}


#ifdef TEST
main(ac, av)
  int ac;
  char *av[];
{
  register i;
  EVENT v;
  long count = 100;

  if (ac == 1) {
    fprintf(stderr, "Usage %s <count> <username> <password <server>\n", av[0]);
    exit (1);
  }
  --ac;
  if (ac >= 1)
    count = strtol(av[1], (char **)NULL, 0);

  if (ac >= 3) {
    username = (text *)av[2];
    password = (text *)av[3];
  }
  if (ac >= 4)
    dbserver = (text *)av[4];

  printf ("Count = %ld, user=%s %s Server=%s\n",
	  count, (char *)username, (char *)password, (char *)dbserver);

  bzero(&v, sizeof(v));
  strcpy(v.sender, "oracleTest");
  strcpy(v.device.name, "test");
  strcpy(v.device.addr,"testaddr");
  strcpy(v.device.subdev, "testsub");
  strcpy(v.var.name, "varname");
  strcpy(v.var.units, "testUnits");
  v.eventtime = time((time_t *)0);

  snips_open_db();
  for (i= 0; i < count; ++i)
  {
    v.var.value = 12345 + i;
    v.var.threshold = 100 + i;
    snips_update_dbevent(&v);
    fprintf(stderr, "."); fflush(stderr);
  }
  fprintf(stderr, "\n");
  snips_close_db();
}
#endif /* TEST */
