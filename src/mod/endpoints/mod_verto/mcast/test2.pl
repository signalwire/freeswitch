use MCAST;



my $action = shift;

if ($action eq "send") {
  my $s = new MCAST::McastHandle("224.1.1.1", 1337, MCAST::MCAST_SEND | MCAST::MCAST_TTL_HOST);
  $s->send(shift);
  print "Sending message";
  exit;
}

my $s = new MCAST::McastHandle("224.1.1.1", 1338, MCAST::MCAST_RECV | MCAST::MCAST_TTL_HOST);

for(;;) {
  my $foo = $s->recv();
  if ($foo) {
    print "RECV [$foo]\n";
  } else {
    print "PING\n";
  }
}
