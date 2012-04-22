#!/usr/bin/perl
use FreeSWITCH::Client;
use Data::Dumper;
use Term::ReadLine;
my $password = "ClueCon";


my $fs = init FreeSWITCH::Client {-password => $password} or die "Error $@";
my $term = new Term::ReadLine "FreeSWITCH CLI";
my $prompt = "[mFreeSWITCH>";
my $OUT = $term->OUT .. \*STDOUT;
my $pid;

my $log = shift;

$SIG{CHLD} = sub {$fs->disconnect(); die "done"};

if ($log) {
  $pid = fork;
  if (!$pid) {
    my $fs2 = init FreeSWITCH::Client {-password => $password} or die "Error $@";
    

    $fs2->sendmsg({ 'command' => "log $log" });
    while (1) {
      my $reply = $fs2->readhash(undef);
      if ($reply->{socketerror}) {
	die "socket error";
      }
      
      if ($reply->{body}) {
	print $reply->{body};
      } 
    }
    exit;
  }

}

while ( defined ($_ = $term->readline($prompt)) ) {
  if ($_) {
    if ($_ =~ /exit/) {
      last;
    }
    my $reply = $fs->command($_);
    if ($reply->{socketerror}) {
      $fs2->disconnect();
      die "socket error";
    }
    print "$reply\n";
    
  }
  $term->addhistory($_) if /\S/;
}
  
if ($pid) {
  kill 9 => $pid;
}
