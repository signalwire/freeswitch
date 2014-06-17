#!/usr/bin/perl
#
# Since the FTC only allows a limited set of numbers to be submitted online
# I've decided to use the power of FreeSWITCH to show how easy this is to solve.
#
# numbers.txt is a file with 1 did per line, 10 digits, If it starts with a 1 thats
# it will be stripped.
#
# Remember, Only use this for numbers YOU personally own.  I have many different DID
#
#

require ESL;
use Data::Dumper;
my $con = new ESL::ESLconnection("localhost", "8021", "ClueCon");
# one 10 digit number PER LINE.
open(NUMS, "<numbers.txt");

my @nums = <NUMS>;

foreach my $num (@nums) {
  chomp $num;
  $num =~ s/^1//;
  print "Dialing from $num\n";
  my @digits = split(//, $num);
  my $dial = join('w', @digits);
  my $command = "originate {ignore_early_media=true,execute_on_answer_01=\'playback silence_strem://1000\',execute_on_answer_02=\'send_dtmf WWWWW1WWWWWWW1WWWWWWW${dial}w#\',execute_on_answer_03='record_session /tmp/$num.wav',origination_caller_id_number=$num}sofia/gateway/59/18883821222 &park";
  my $e = $con->api('bgapi', "$command");
  print $e->getBody();
  sleep 40;
}
close(NUMS);
print "Done...";











