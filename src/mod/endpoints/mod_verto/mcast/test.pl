use MCAST;

my $s = new MCAST::McastHandle("231.3.3.7", 1337, MCAST_SEND | MCAST_RECV | MCAST_TTL_HOST);

my $action = shift;

if ($action eq "send") {
  $s->send("W00t from Perl " . shift);
  exit;
}

for(;;) {
  my $foo = $s->recv(100);
  if ($foo) {
    print "RECV [$foo]\n";
  } else {
    print "PING\n";
  }
}
