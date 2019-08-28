#!/usr/bin/perl
# Just a stress program to measure some timings.
#
BEGIN {
  push (@INC, "/usr/local/snips/lib/perl");
}
use SNIPS;

my $test = "read2";	# read write1 or write2
sub test {
  my ($datafile) = @_;
  my $event, %event;

  $datafd = open_datafile($datafile, "r+");
  while ( ($event = read_event($datafd)) )
  {
    if ($test eq "read1") {
    }
    elsif ($test eq "read2") {
      @fields = SNIPS::get_eventfields();
      @event = SNIPS::event2array($event);
      print join (", ", @fields), "\n";
      my $j = 0;
      foreach my $i (@fields) {	$$i = $j; ++$j;}
      print "Sender $event[$sender] and varname $event[$var_name]\n";
      print join (", ", @event), "\n";
      exit 0;
    }
    elsif ($test eq "write1") {
      alter_event($event, "newSender", "newDevice", "newAddr", "boot", "hoo");
      rewrite_event($datafd, $event);
    }
    elsif ($test eq "write2") {
      %event = unpack_event($event);
      #$event{sender} = "newSender";
      #$event{device_name} = "newDevice";
      #$event{device_addr} = "newAddr";
      #$event{var_name} = "boo";
      #$event{var_units} = "hoo";
      #$event = pack_event(%event);
      #rewrite_event($datafd, $event);
    }
    print ".";
  }
  close_datafile($datafd);
}

if ($#ARGV < 0) {die "USAGE: $0 datafile...\n"; }
foreach (@ARGV) { print "$_\n"; &test($_); }
print "\n";
