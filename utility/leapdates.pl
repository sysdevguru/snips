#!/usr/local/bin/perl

sub cvt_time {
  local ($timestr) = @_;	# Mon Aug 31 16:01:32 EDT 1998
  %monthshash = ("Jan", 0, "Feb", 1, "Mar", 2, "Apr", 3, "May", 4, "Jun", 5,
		 "Jul", 6, "Aug", 7, "Sep", 8, "Oct", 9, "Nov", 10, "Dec", 11);
  # offset in days of months in the year.
  %days_offset = ('Jan', 0,    'Feb', 31,   'Mar', 59,  'Apr', 90,
		   'May', 120,  'Jun', 151,  'Jul', 181, 'Aug', 212,
		   'Sep', 243,  'Oct', 273,  'Nov', 304, 'Dec', 334);

  local ($junk, $smon, $day, $hr, $min, $sec, $year) =
    split(/[:\s]+/, $timestr);


  local ($mon) = $monthshash{$smon};
  print "$timestr    $smon, $mon, $day, $hr, $year\n" ;
  print "$leaps, $year\n";
  $age = (($year - 1970)*365) + $days_offset{$smon} + ($day - 1) + $leaps;
  $age *= (24 * 3600);
  $age += ($hr * 3600) + ($min * 60) + $sec;

  return ($age);

}

$x = localtime;
$a = &cvt_time($x);
$b = time;
printf "%ld %ld %ld\n", $a, time, time - $a;
$y = localtime($a);
print "$x\n$y\n";
$y = localtime(time);
print "$y\n";
