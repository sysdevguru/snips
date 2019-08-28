/* $Header: /home/cvsroot/snips/lib/odbc_api.c,v 0.1 2001/07/08 22:22:41 vikas Exp $ */

/*
 *  INCOMPLETE WORK.
 */

/*
 * ODBC interface to SNIPS
 *
 *	snips_open_db()
 *	snips_close_db()
 *	snips_insert_dbevent()
 *	snips_update_dbevent()
 *	snips_read_dbevent()
 *
 * You can link with the iODBC library from www.mysql.com.
 * 	setenv LD_LIBRARY_PATH /usr/local/iodbc/lib
 *
 *  1. setenv ODBCINI  odbc.ini
 *  2. Create odbc.ini
        ;
        ;  odbc.ini
        ;
        [ODBC Data Sources]
        SNIPS           = SNIPS Database
        Sales           = Sales Database

        [SNIPS]
        Driver          = /usr/local/mysql/MyODBC/lib/libmyodbc.so
        Description     = SNIPS Database
        Server          = localhost
        User            = openlink
        Password        = xxxx
        Database        = snips
        ReadOnly        = no
        TraceFile       = /tmp/odbc.trace
        Trace           = 1
 *
 *
 *
 * AUTHOR:
 *		Vikas Aggarwal, vikas@navya.com
 */

/*
 * $Log: odbc_api.c,v $
 * Revision 0.1  2001/07/08 22:22:41  vikas
 * For SNIPS interface to databases. Incomplete work.
 *
 */


/* #define ODBCVER 0x300	/* force to version 3.0, not 3.5 */

#include <sql.h>	/* ODBC include file */
#include <sqlext.h>	/* ??? */
#include "../include/snips.h"	/* FIX FIX */

#ifdef TEST
int	insert = 1;
#endif

#ifdef SQL_OV_ODBC3
# define ODBC_V3	/* which version of odbc supported by the driver */
#else
# define ODBC_V2
#endif

/*------------------------ Global Variables -------------------------------*/

static	SQLCHAR *username = (SQLCHAR *)NULL;	/* get it from the DSN */
static	SQLCHAR *password = (SQLCHAR *)NULL;
static	SQLCHAR *dbserver = (SQLCHAR *)"SNIPS";		/* DSN */
/* the database name is snips_events   set in the sqlstmt below */

SQLHENV 	henv;
SQLHDBC 	hdbc;
SQLHSTMT	hstmtu, hstmti;	/* for update, insert */
SQLRETURN	retcode;

static int	free_sqlhandles(), print_dberrors();
static int	bind_update_parameters(), bind_insert_parameters();
static int	logged_on;
static int	sdebug = 0;	/* local debug */

/*========================== API FUNCTIONS ======================*/

snips_open_db()
{
  if (sdebug) fprintf(stderr, "debug:: snips_open_db()\n");
  if (logged_on)
    return (1);

  /*** Allocate environment handle ***/
#ifdef ODBC_V3
  if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv) != SQL_SUCCESS) {
#else
  if (SQLAllocEnv(&henv) != SQL_SUCCESS) {
#endif
    fprintf(stderr, "SQLAllocHandle(ENV) failed\n");
    return (print_dberrors());
  }
#ifdef ODBC_V3
  SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
#endif

  /*** Allocate connection handle ***/
#ifdef ODBC_V3
  if (SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc) != SQL_SUCCESS) {
#else
  if (SQLAllocConnect(henv, &hdbc)) {
#endif
    fprintf(stderr, "SQLAllocHandle(DBC) failed\n");
    print_dberrors();
    free_sqlhandles();
    return (-1);
  }

#ifdef ODBC_V3
/*  SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (void *)7, 0);	/* FIX 7 secs */
#else
  SQLSetConnectOption(hdbc, SQL_LOGIN_TIMEOUT, 7);	/* 7 secs */
#endif

  retcode = SQLConnect(hdbc, dbserver, SQL_NTS, username, SQL_NTS,
		       password, SQL_NTS);

  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    fprintf(stderr, "Error in SQLConnect() (retcode = %d)\n", retcode);
    print_dberrors();
    free_sqlhandles();
    return (-1);
  }

  /* SQLSetStmtAttr();	/*	*/

  logged_on = 1;
  return (1);
}	/* snips_open_db() */

snips_close_db()
{
  if (sdebug) fprintf(stderr, "debug:: snips_close_db()\n");
  if (logged_on)
  {
    SQLDisconnect(hdbc);
    free_sqlhandles();
  }

  return (0);
}

/*
 * Allocate an insert statement and also a update statement. Can just change
 * which statement is executed by changing the SQLExecute() line.
 */
snips_update_dbevent(pv)
  EVENT *pv;
{
  /* dont change this statement without updating bind_input() */
  SQLCHAR  *sqlstmt_u = 
    "UPDATE snips_events SET VAR_VALUE = ?, \
     VAR_THRESHOLD = ?, SEVERITY = ?, \
     LOGLEVEL = ?, STATE = ?, RATING = ?  \
     WHERE SENDER = ? AND DEVICE_NAME = ?  \
     AND DEVICE_SUBDEV = ? AND VAR_NAME = ?";

  /* this is an INSERT statement instead of an UPDATE */
  SQLCHAR *sqlstmt_i =
    "INSERT INTO snips_events (SENDER,  \
     DEVICE_NAME, DEVICE_ADDR, DEVICE_SUBDEV,  \
     VAR_NAME, VAR_VALUE, VAR_THRESHOLD, VAR_UNITS, \
     SEVERITY, LOGLEVEL, STATE, RATING, EVENTTIME, POLLTIME,
     OP, ID) \
     VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  /* if (sdebug) fprintf(stderr, "debug:: snips_update_dbevent()\n");	/* */
  if (!hstmtu)
  {
    if (snips_open_db() < 0)
      return (-1);

#ifdef ODBC_V3
    if (SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmtu) != SQL_SUCCESS ||
	SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmti) != SQL_SUCCESS)
#else
    if (SQLAllocStmt(hdbc, &hstmtu) != SQL_SUCCESS ||
	SQLAllocStmt(hdbc, &hstmti) != SQL_SUCCESS)
#endif
    {
      return (print_dberrors());
    }

    if ( (retcode = SQLPrepare(hstmtu, sqlstmt_u, SQL_NTS)) == SQL_SUCCESS )
      retcode = SQLPrepare(hstmti, sqlstmt_i, SQL_NTS);

    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      fprintf(stderr, "Error in preparing SQL statement\n");
      print_dberrors();
#ifdef ODBC_V3
      SQLFreeHandle(SQL_HANDLE_STMT, hstmtu);
      SQLFreeHandle(SQL_HANDLE_STMT, hstmti);
#else
      SQLFreeStmt(hstmtu, SQL_DROP);
      SQLFreeStmt(hstmti, SQL_DROP);
#endif
      hstmti = hstmtu = NULL;
      return (-1);
    }
    bind_insert_parameters(hstmti, pv);	     	/* inserts */
    bind_update_parameters(hstmtu, pv);		/* updates */
  }

  if (insert)
    retcode = SQLExecute(hstmti);	/* insert */
  else
    retcode = SQLExecute(hstmtu);	/* update */

  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
    fprintf(stderr, "SQLExecute() failed (retcode= %d)\n", retcode);
    return (print_dberrors());
  }
#ifdef ODBC_V3
  SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);	/* FIX, only at end? */
#else
  SQLTransact(henv, hdbc, SQL_COMMIT);	/* FIX, only at end? */
#endif
}	/* snips_update_dbevent() */

/*
 * To be completed FIX FIX
 */
snips_get_dbevent()
{
  /* Fetch the first row. */
/*
  SQLBindCol(hstmt, 1, SQL_C_SLONG, &id, 0, NULL);
  SQLBindCol(hstmt, 2, SQL_C_CHAR, name, (SDWORD)sizeof(name), &namelen);
  SQLFetch(hstmt);
*/
#ifdef ODBC_V3
  SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
#else
  SQLTransact(henv, hdbc, SQL_COMMIT);
#endif
}

/*
 * This function is not really used, but just for testing
 */
snips_create_table()
{
  int ret = 0;
  SQLHSTMT hstmt;
  SQLCHAR  *sqlstmt = 
     "CREATE TABLE snips_events ( \
        sender char(40) NOT NULL, \
        device_name char(128) NOT NULL, device_addr char(128), \
        device_subdev char(128) DEFAULT '' NOT NULL, \
        var_name char(64) NOT NULL, \
        var_value integer, var_threshold integer, \
        var_units char(64), \
        severity integer, loglevel integer, state integer, rating integer, \
        eventtime integer, polltime integer, \
        op integer, id integer primary key)";

  fprintf(stderr, "snips_create_table(snips_events)\n");

  if (snips_open_db() < 0)
    return (-1);

#ifdef ODBC_V3
  if (SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt) != SQL_SUCCESS)
#else
  if (SQLAllocStmt(hdbc, &hstmt) != SQL_SUCCESS)
#endif
  {
    return (print_dberrors());
  }

  fprintf(stderr, "  Dropping table snips_events..");
  SQLExecDirect(hstmt, "DROP TABLE snips_events", SQL_NTS);
#ifdef ODBC_V3
  SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
#else
  SQLTransact(henv, hdbc, SQL_COMMIT);
#endif
  fprintf(stderr, "done\n");

  retcode = SQLExecDirect(hstmt, sqlstmt, SQL_NTS);
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    fprintf(stderr, "Error in executing SQL statement\n");
    print_dberrors();
    ret = -1;
  }

#ifdef ODBC_V3
  SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
#else
  SQLTransact(henv, hdbc, SQL_COMMIT);
#endif

#ifdef ODBC_V3
  SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
#else
  SQLFreeStmt(hstmt, SQL_DROP);
#endif

  return (ret);
}	/* create_table() */


/* ==================  INTERNAL Support Functions ================= */

static
bind_update_parameters(stmt, pv)
  SQLHSTMT stmt;
  EVENT *pv;
{
  if (sdebug) fprintf(stderr, "debug:: bind_update_parameters()\n");
  /* value */
  if (
      SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->var.value), 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->var.threshold), 0, NULL) != SQL_SUCCESS
      /* severity */
      ||
      SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_UTINYINT, SQL_SMALLINT,
		       0, 0, &(pv->severity), 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_UTINYINT, SQL_SMALLINT,
		       0, 0, &(pv->loglevel), 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_UTINYINT, SQL_SMALLINT,
		       0, 0, &(pv->state), 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_UTINYINT, SQL_SMALLINT,
		       0, 0, &(pv->rating), 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       0, 0, pv->sender, 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       0, 0, pv->device.name, 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       0, 0, pv->device.subdev, 0, NULL) != SQL_SUCCESS
      ||
      SQLBindParameter(stmt, 10, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       0, 0, pv->var.name, 0, NULL) != SQL_SUCCESS
      )
  {
    return (print_dberrors());
  }

  return (0);
}

static
bind_insert_parameters(stmt, pv)
  SQLHSTMT stmt;
  EVENT *pv;
{
  SQLINTEGER zero = 0, nullc = SQL_NTS;	/* for StrLen_or_IndPtr arg */

  if (sdebug) fprintf(stderr, "debug:: bind_insert_parameters()\n");
  if (
      SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       sizeof(pv->sender), 0, pv->sender, 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       sizeof(pv->device.name), 0, pv->device.name, 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       sizeof(pv->device.addr), 0, pv->device.addr, 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       sizeof(pv->device.subdev), 0, pv->device.subdev, 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       sizeof(pv->var.name), 0, pv->var.name, 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->var.value), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->var.threshold), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
		       sizeof(pv->var.units), 0, &(pv->var.units), 0, NULL)
      != SQL_SUCCESS ||
      /* severity */
      SQLBindParameter(stmt, 9, SQL_PARAM_INPUT, SQL_C_BIT, SQL_INTEGER,
		       0, 0, &(pv->severity), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 10, SQL_PARAM_INPUT, SQL_C_BIT, SQL_INTEGER,
		       0, 0, &(pv->loglevel), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 11, SQL_PARAM_INPUT, SQL_C_BIT, SQL_INTEGER,
		       0, 0, &(pv->state), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 12, SQL_PARAM_INPUT, SQL_C_BIT, SQL_INTEGER,
		       0, 0, &(pv->rating), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 13, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->eventtime), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 14, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->polltime), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 15, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->op), 0, NULL)
      != SQL_SUCCESS ||
      SQLBindParameter(stmt, 16, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER,
		       0, 0, &(pv->id), 0, NULL)
      )
  {
    return (print_dberrors());
  }

  return (0);
}	/* bind INSERT parameters */

static
free_sqlhandles()
{
#ifdef ODBC_V3
  /* if (hstmtu) SQLFreeHandle(SQL_HANDLE_STMT, hstmtu); FIX	*/
  /* if (hstmti) SQLFreeHandle(SQL_HANDLE_STMT, hstmti);	*/
  if (hdbc)  SQLFreeHandle(SQL_HANDLE_DBC,  hdbc);
  if (henv)  SQLFreeHandle(SQL_HANDLE_ENV,  henv);
#else
  if (hstmtu) SQLFreeStmt(hstmtu, SQL_DROP);
  if (hstmti) SQLFreeStmt(hstmti, SQL_DROP);
  if (hdbc)  SQLFreeConnect(hdbc);
  if (henv)  SQLFreeEnv(henv);
#endif

  henv = hdbc = hstmtu = hstmti = NULL;
}

static
print_dberrors()
{
#ifdef ODBC_V2
  unsigned char sqlstate[15];
  char buf[250];	/* has to be 250 for whatever reason */

  /* Get statement errors */
  while (hstmti && SQLError (henv, hdbc, hstmti, sqlstate, NULL,
			     buf, sizeof(buf), NULL) == SQL_SUCCESS)
    fprintf (stderr, "%s, SQLSTATE=%s\n", buf, sqlstate);

  while (hstmtu && SQLError (henv, hdbc, hstmtu, sqlstate, NULL,
			     buf, sizeof(buf), NULL) == SQL_SUCCESS)
    fprintf (stderr, "%s, SQLSTATE=%s\n", buf, sqlstate);

  /* Get connection errors */
  while (SQLError (henv, hdbc, SQL_NULL_HSTMT, sqlstate, NULL,
		   buf, sizeof(buf), NULL) == SQL_SUCCESS)
    fprintf (stderr, "%s, SQLSTATE=%s\n", buf, sqlstate);

  /* Get environmental errors */
  while (SQLError (henv, SQL_NULL_HDBC, SQL_NULL_HSTMT, sqlstate, NULL,
		   buf, sizeof(buf), NULL) == SQL_SUCCESS)
    fprintf (stderr, "%s, SQLSTATE=%s\n", buf, sqlstate);

#else	/* ODBC_V3 */
  SQLSMALLINT	recNumber = 1;
  SQLCHAR	sqlstate[6];
  SQLCHAR	buf[256];
  SQLSMALLINT	actualLen;

  sqlstate[5] = '\0';
  /* Get the diagnostics for the error */
  while (SQLGetDiagRec(SQL_HANDLE_ENV, henv, recNumber++, 
		       &sqlstate[0], NULL, &buf[0], sizeof(buf),
		       &actualLen) == SQL_SUCCESS)
    fprintf(stderr, "(env) %s, SQLSTATE=%s\n", buf, sqlstate);

  recNumber = 1;
  while (SQLGetDiagRec(SQL_HANDLE_DBC, hdbc, recNumber++, 
		       &sqlstate[0], NULL, &buf[0], sizeof(buf),
		       &actualLen) == SQL_SUCCESS)
    fprintf(stderr, "(connection) %s, SQLSTATE=%s\n", buf, sqlstate);

  recNumber = 1;
  while (SQLGetDiagRec(SQL_HANDLE_STMT, hstmtu, recNumber++, 
		       &sqlstate[0], NULL, &buf[0], sizeof(buf),
		       &actualLen) == SQL_SUCCESS)
    fprintf(stderr, "(stmt) %s, SQLSTATE=%s\n", buf, sqlstate);
  recNumber = 1;
  while (SQLGetDiagRec(SQL_HANDLE_STMT, hstmti, recNumber++, 
		       &sqlstate[0], NULL, &buf[0], sizeof(buf),
		       &actualLen) == SQL_SUCCESS)
    fprintf(stderr, "(stmt) %s, SQLSTATE=%s\n", buf, sqlstate);

#endif /* ODBC_V2 */

  return -1;
}	/* print_dberrors() */


#ifdef TEST
main(ac, av)
  int ac;
  char *av[];
{
  register long i;
  long count = 1000;
  char *prog;
  extern char *optarg;
  extern int optind;
  int c;
  EVENT v;

  prog = av[0];
  --ac;
  if (ac == 0) {
      fprintf (stderr, "Usage %s [-i|-u|-d] <count> <user> <password> <DSN>\n", av[0]);
    fprintf (stderr, "If -i (insert mode) is specified, will create table %s\n", (char *)dbserver);
    exit (1);
  }

  while ((c = getopt(ac, av, "diu")) != EOF)
    switch (c)
    {
    case 'i':
      insert = 1;
      break;
    case 'u':
      insert = 0;
      break;
    case 'd':
      sdebug = 1;
      break;
    default:
      fprintf(stderr, "unknown flag %c\n", av[1][1]);
      fprintf (stderr, "Usage %s [-i|-u|-d] <count> <user> <password> <DSN>\n", av[0]);
      exit (1);
    }

  if (ac >= optind)
    count = strtol(av[optind++], (char **)NULL, 0);
  if (ac >= optind)
    username = (SQLCHAR *)av[optind++];
  if (ac >= optind)
    password = (SQLCHAR *)av[optind++];
  if (ac >= optind)
    dbserver = (SQLCHAR *)av[optind++];

  printf ("Count = %ld, user=%s %s DSN=%s\n",
	  count, (char *)username, (char *)password, (char *)dbserver);

  bzero(&v, sizeof(v));
  strcpy(v.sender, "odbcTest");
  strcpy(v.device.name, "test");
  strcpy(v.device.addr,"testaddr");
  strcpy(v.device.subdev, "testsub");
  strcpy(v.var.name, "varname");
  strcpy(v.var.units, "testUnits");
  v.eventtime = time((time_t *)0);

  if (insert == 1 && snips_create_table() < 0)
    exit (-1);
  if (snips_open_db() < 0)
    exit (-1);

  fprintf(stderr, "%s\n", insert == 1 ? "INSERTING..." : "UPDATING...");

  for (i= 0; i < count; ++i)
  {
    /* fprintf(stderr, "%d ", i); fflush(stderr);	/* */
    sprintf(v.device.name, "device%d", i);
    sprintf(v.device.subdev, "sub%d", i);
    v.var.value = 1000 + i;
    v.var.threshold = 2000 + i;
    v.op = i;
    v.id = i;
    snips_update_dbevent(&v);
    if (v.op % 1000 == 0) {
      fprintf(stderr, "%ld ", i); 
      fflush (stderr);
    }
  }
  
#ifdef ODBC_V3
  SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
#else
  SQLTransact(henv, hdbc, SQL_COMMIT);
#endif
  snips_close_db();

  fprintf(stderr, "\nDONE\n");
}
#endif /* TEST */
