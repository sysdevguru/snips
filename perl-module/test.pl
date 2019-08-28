# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

# $Header: /home/cvsroot/snips/perl-module/test.pl,v 1.0 2001/07/09 04:17:28 vikas Exp $

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..1\n"; }
END {print "not ok 1\n" unless $loaded;}
use SNIPS;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

$SNIPS::debug = 2;
$SNIPS::dorrd = 3;
$SNIPS::doreload = 4;
# $SNIPS::dummy = 9;
$SNIPS::s_configfile = "ConfigFile";
$SNIPS::s_datafile = "DataFile";
print "dorrd is: ", $SNIPS::dorrd,
  ", debug is: ",   $SNIPS::debug,
  ", doreload is: ", $SNIPS::doreload,
  ", configfile is: ", $SNIPS::s_configfile,
  ", datafile is: ", $SNIPS::s_datafile,
  "\n";

$av = SNIPS::new_event();
$bv = SNIPS::new_event();
SNIPS::init_event($av);
SNIPS::init_event($bv);

SNIPS::alter_event($av, "A1", "A2", "A3", "A4", "A5", "A6");
SNIPS::alter_event($bv, "B1", "B2", "B3", "B4", "B5", "B6");


###
SNIPS::main(\&readconf, undef, \&do_test);  #never returns
#SNIPS::startup();
$datafile = $SNIPS::s_datafile;
$configfile = $SNIPS::s_configfile;
print "\t Datafile = $datafile\n\t Configfile= $configfile\n";

print "Testing event functions:\n";

@fields = SNIPS::get_eventfields();
print "Event fields are:\n   ", join (" ", @fields), "\n";

($status, $threshold, $maxseverity) = SNIPS::calc_status(2, 1, 3, 5);
print "\nTested calc_status()\n";


print "\nNow reading back events written to $datafile\n";

$datafd = SNIPS::open_datafile($datafile, "r");
while ( ($event = SNIPS::read_event($datafd) ) )
{
  %event = SNIPS::unpack_event($event);
  print "--------\n";

  foreach $key (keys %event) {
    print "$key = $event{$key} ; ";
  }
  print "\n";

  print "Now extracting event using event2array()\n";
  @event = SNIPS::event2array($event);
  my $i = 0;
  foreach my $f (@fields) { 
    $$f = $i;
    ++$i;
    print "$f = $event[$$f] ; ";
  }

  print "REPACKING EVENT\n";
  $tevent = SNIPS::pack_event(%event);
  # SNIPS::print_event($tevent);

  print "Comparing event before and after packing  ";
  %tevent = SNIPS::unpack_event($tevent);
  foreach $key (keys %event) {
    if ($event{$key} != $tevent{$key}) {
      print "\nFAILED key= $key, $event{$key} != $tevent{$key}\n";
    }
    else { 
      print ".";
    }
  }
  print "DONE\n";

}

SNIPS::close_datafile($datafd);

if (SNIPS::get_reload_flag() > 0) {
  SNIPS::reload(\&readconf);
}

#SNIPS::done();

sub readconf {
  print "This is subroutine readconf(), file= $s_datafile\n";
  $datafd = SNIPS::open_datafile($s_datafile, "w");
  
  $event = SNIPS::new_event();
  print "\tgot new_event\n";
  
  SNIPS::init_event($event);
  print "\tdone init_event\n";
  
  SNIPS::update_event($event, 1, 54321, 4444, $E_WARNING);
  print "\tdone update_event\n";
  print_event($event);
  SNIPS::update_event($event, 1, 66666, undef, $E_CRITICAL);
  print_event($event);
  
  $bytes = SNIPS::write_event($datafd, $event);
  print "\tdone write_event ($bytes bytes)\n";
  
  SNIPS::update_event($event, 1, 12345, 2345, $E_WARNING);
  $bytes = SNIPS::write_event($datafd, $event);
  print "\tdone write_event ($bytes bytes)\n";
  
  SNIPS::close_datafile($datafd);
  print "DONE\n\n";

}

sub do_test {
  my ($event, $deviceno) = @_;

  print "This is function dotest\n";
  my ($status, $thres, $maxsev) = SNIPS::calc_status(2, 1, 3, 5);
  return($status, 2, $thres, $maxsev);

}
