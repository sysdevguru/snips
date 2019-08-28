#!/usr/local/bin/perl5 -s
#
# Create SNMPApps manpages. Given a list of manpage templates, this 
# script generates everything.  I use this script, so that the common
# features of all manpages always remain common.  (IE: "See Also",
# "Environment", etc.)
#
# Author: Ryan Troll <ryan@andrew.cmu.edu>
#
# $Id: make-manpages.pl,v 1.1 1998/05/07 21:54:14 ryan Exp $
#
######################################################################

@Pages = @ARGV;

########################################################################

foreach $Page (@ARGV) {
  print STDOUT "Generating manpage for $Page...";
  &GenPage($Page, "${Page}.man", "${Page}.1");
  print STDOUT "Done.\n";
}
exit(0);

########################################################################

sub GenPage {
  my($Name, $Src, $Dest) = @_;

  open(MANOUT, "> ${Dest}") ||
    die "Unable to write to ${Dest}: $!";

  &Header($Name);
  &Cat($Src);
  &Environment();
  &Files();
  &RFCs();
  &URLs();
  &SeeAlso();
  close(MANOUT);
}


########################################################################

sub Cat {
  my($F) = @_;

  open(A, $F) || die "Unable to read $F: $!";
  while(<A>) {
    print MANOUT $_;
  }
  close(A);
}

########################################################################

sub Header {
  my($Page) = @_;
  my($Time) = scalar localtime;

  $Page =~ y/a-z/A-Z/;
  print MANOUT ".TH $Page 1 \"$Time\"\n";
  print MANOUT ".UC 4\n";
}

########################################################################

sub Environment {

  print MANOUT ".SH \"ENVIRONMENT\"\n";

  ## Environment variable from the CMU SNMP Library
  ##
  print MANOUT "MIBFILE:  Location of the SNMP MIB.\n";

}

########################################################################

sub Files {
  print MANOUT ".SH \"FILES\"\n";

  ## Files used by the CMU SNMP Library
  ##
  print MANOUT ".nf\n";
  print MANOUT "mib.txt                   First MIB tried if env. var is not set\n";
  print MANOUT "/etc/mib.txt              Second MIB tried if env. var is not set\n";
}

########################################################################

sub RFCs {
  print MANOUT ".SH \"RFCS\"\n";

  print MANOUT "Related RFCs: 1065, 1066, 1067\n";
  print MANOUT ".br\n";
  print MANOUT "Related SNMPv2 RFCs: 1901, 1902, 1902, 1904, 1905, 1906, 1907, 1908, 1909\n";
}

########################################################################

## Related URLs
##
sub URLs {
  print MANOUT ".SH \"RELATED URLS\"\n";
  print MANOUT "CMU Networking Group: http://www.net.cmu.edu/\n";
  print MANOUT ".br\n";
  print MANOUT "CMU SNMP Home Page: http://www.net.cmu.edu/projects/snmp\n";
}

########################################################################

sub SeeAlso {
  my($l, $n, $s);
  print MANOUT ".SH \"SEE ALSO\"\n";
  
  $l = 0;
  $s = 0;
  foreach $I (sort(@Pages)) {
    $n = "${I}(1)";
    $l += length($n);

    if ($l > 77) {
      $l = length($n);
      print MANOUT ",\n";
    } else {
      if ($s > 0) {
	print MANOUT ", ";
      }
      $s++;
      $l += 2;
    }
    print MANOUT $n;
  }
  print MANOUT "\n";
}

########################################################################
