#!/usr/bin/perl
# Program to strip the mib-file of all the DESCRIPTIONS
#
#	-vikas@navya.com

my $doskip = 0;

while(<>)
{
  if (/^\s*DESCRIPTION\s*$/) {
    $doskip = 1;
    next;
  }
  if ($doskip && /\"\s*$/) {
    $doskip = 0;
    next;
  }
  next if ($doskip);
  s/\n?/\n/g;		# strip off any ctrl-Ms
  print;
}
