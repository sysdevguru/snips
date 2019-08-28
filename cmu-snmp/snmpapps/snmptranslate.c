/***************************************************************************
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
 * Author: Ryan Troll <ryan+@andrew.cmu.edu>
 * 
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#ifdef WIN32
#include <memory.h>
#include <winsock2.h>
#else  /* WIN32 */
#include <netinet/in.h>
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

static char rcsid[] = 
"$Id: snmptranslate.c,v 1.8 1998/08/06 14:23:22 ryan Exp $";

void Version(void)
{
  fprintf(stderr, "snmptranslate, version %s\n", snmpapps_Version());
  fprintf(stderr, "\tUsing CMU SNMP Library, version %s\n", snmp_Version());
  exit(1);
}

void Usage(void)
{
  fprintf(stderr, "usage:  snmptranslate [-n] [-d] object-identifier\n");
  exit(-1);
}

void main(int argc, char **argv)
{
  int arg;
  char *current_name = NULL;
  oid name[MAX_NAME_LEN];
  int name_length = MAX_NAME_LEN;
  int tosymbolic = 0;
#if 0
  int description = 0;
#endif
  int Verbose = 0;

  /* Usage: snmptranslate name
   */

  for(arg = 1; arg < argc; arg++) {
    if (argv[arg][0] == '-') {
      switch(argv[arg][1]) {
      case 'n':
	tosymbolic = 1;
	break;       
      case 'v':
	Verbose = 1;
	break;       
#if 0
      case 'd':
	description = 1;
	break;
#endif
      case 'V':
	Version();
	break;
      default:
	printf("invalid option: -%c\n", argv[arg][1]);
	break;
      }
      continue;
    }
    current_name = argv[arg];
  }

  if (current_name == NULL) {
    Usage();
  }

  /* Load the MIB */
  init_mib();

  /* First, let's convert this to a true OID */
  if (!read_objid(current_name, name, &name_length)) {
    printf("Invalid name '%s'!\n", current_name);
    exit(-1);
  }

  if (Verbose) {
    printf("Name: '%s'\n", current_name);
  }

  if (tosymbolic) {
    /* Numeric */
    if (Verbose) 
      printf("Nums:  ");
    print_oid_nums(name, name_length);
    printf("\n");
  } else {
    /* Lots of words */
    if (Verbose) 
      printf("Words: ");
    print_objid(name, name_length);
  }

#if 0
  if (description)
    print_description(name, name_length);
#endif
}

