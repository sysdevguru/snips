package Monitor::Hostmon;
#
# This connects to a host's 5355 port, splits the output and reports it.
#
use Monitor;
use Socket;
use Sys::Hostname;
use strict;
use vars qw{ @ISA };

@ISA = qw{ Monitor };

sub initialize {
  my $self = shift;
  my $success = 1;
  $self->{"monitor"} = "hostmon";
  return $success && $self->SUPER::initialize(@_);
}

sub run {
  my $self = shift;
  my($time) = @_;
  my($host,$raw,$line,$starttime,%down);
  my($proto,$port,$count,$rin,$hostaddr);

# Errors:
#   1 -- Host never replied (up but inetd not responding)
#   2 -- Replied but result is really out of date (hostmond process down)
#   3 -- Couldn't send packet to host (not up)


  $proto = getprotobyname('udp') or die "Couldn't lookup protocol UDP: $!";
  $port  = getservbyname("hostmon","udp") or die "Couldn't lookup service hostmon: $!";
  socket(SOCKFD, AF_INET, SOCK_DGRAM, $proto) or die "Couldn't create socket: $!";
  bind(SOCKFD, sockaddr_in(0,inet_aton(hostname()))) or die "Couldn't bind: $!";
  $|=1;

  my $cnt = 0;
  my $count = 'true';
  warn "Starting loop\n";
  while (($cnt++<2) && $count) {
    warn "In loop\n";
    foreach $host (@{$self->{"ip"}}) {
      if ($down{$host} ne '0' && $down{$host} ne '2') {
        # Note: It doesn't actually matter what you send, the server process responds
        #       to any packets.  However it's 'recv' is for 10 bytes so length might
        #       matter.
        print STDERR "Trying to send to $host: $port: $down{$host}: ";
        if (send(SOCKFD,"GIMME IT!!",0,sockaddr_in($port,inet_aton($host)))) {
          if ($down{$host} != 1) {
            $down{$host} = 1;
            $count ++;
          }
          print STDERR "Success!\n";
        } else {
          $down{$host} = 3;
          print STDERR "Failure!\n";
        }
      }
    }
    warn "data sent\n";
    vec($rin,fileno(SOCKFD),1) =1;
    while ($count && select($rin,undef,undef,10)) {
      ($hostaddr = recv(SOCKFD,$raw,4096,0)) or do{$count --; print STDERR "RECV: $hostaddr $!\n"; next};
      my($port,$host) = sockaddr_in($hostaddr);
      $host = inet_ntoa($host);
      $self->{"db"}->select("state","value",{type=>$self->{"monitor"},ip_addr=>$host,interface=>"TIME"});
      my(%TIME) = $self->{'db'}->fetchhash;
      print STDERR "Processing $host: ";
      foreach $line (split(/\n/,$raw)) {
        my($interface,$value,$unit,$interface_info)=split(/ /,$line);
        if ($interface eq "TIME") {
          print STDERR "Time was: $TIME{value}  Time is: $value\n";
          if ($value eq $TIME{"value"}) {
            $down{$host} = 2;
          } else {
            $down{$host} = 0;
          }
        }
        $self->report($host,$interface,$interface_info,$value,$unit,$time);
      }
      $count --;
    }
  }
  foreach (keys(%down)) {
    $self->report($_,"hostmond","Down",$down{$_},"",$time);
    print STDERR "Final report: $_: $down{$_}\n";
  }
    
  # Delete all mail queue destinations for this host that weren't updated in the last 15 minutes.
  $self->{"db"}->delete("state",{"update_time < "=>$self->{"db"}->timetosqlts($time-900),interface=>"MailQDest"});
}
