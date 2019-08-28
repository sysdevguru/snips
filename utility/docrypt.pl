#!/usr/local/bin/perl
#
# Given a password, encrypt it so that we can enter the string in a
# password style file. use to maintain 'webusers'
#
#	-vikas@navya.com
#

@a = (A..Z, a..z);

$salt = $a[($$ % 52)];
$salt .= $a[(time % 52)];

# system "stty -echo";
print "Characters will be echoed\n";
print "Password:  ";
chop($inword = <STDIN>);
print "Encrypted: ";
system "stty echo";
print (crypt($inword, $salt)) ;
print "\n";

