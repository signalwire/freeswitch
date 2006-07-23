use FreeSWITCH::Client;
use Data::Dumper;
use Term::ReadLine;
my $password = "ClueCon";


my $fs = init FreeSWITCH::Client {-password => $password} or die "Error $@";
my $term = new Term::ReadLine "FreeSWITCH CLI";
my $prompt = "FS>";
my $OUT = $term->OUT .. \*STDOUT;

my $log = shift;

if ($log) {
  $pid = fork;
  if (!$pid) {
    my $fs2 = init FreeSWITCH::Client {-password => $password} or die "Error $@";
    $fs2->cmd("log $log");
    while (1) {
      my $reply = $fs2->readhash(undef);
      
      if ($reply->{body}) {
	print $reply->{body} . "\n";
      } elsif ($reply->{'reply-text'}) {
	print $reply->{'reply-text'} . "\n";
      }
    }
  }
}



while ( defined ($_ = $term->readline($prompt)) ) {
  my $reply;

  if ($_) {
    my $reply = $fs->cmd("api $_");
      
    if ($reply->{body}) {
      print $reply->{body};
    } elsif ($reply->{'reply-text'}) {
      print $reply->{'reply-text'};
    }
    print "\n";
    if ($_ =~ /exit/) {
      last;
    }
  }
  $term->addhistory($_) if /\S/;
}
  
