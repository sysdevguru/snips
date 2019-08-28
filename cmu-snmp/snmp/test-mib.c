#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include "asn1.h"
#include "parse.h"

void main(int argc, char **argv)
{
  init_mib();
  printf("MIB Initialized\n");
}
