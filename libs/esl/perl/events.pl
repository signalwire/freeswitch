require ESL;

my $con = new ESL::ESLconnection("localhost", "8021", "ClueCon");

$con->events("plain", "all");

for(;;) {
  #my $e = $con->recvEventTimed(100);
  my $e = $con->recvEvent();

  if ($e) {
    print $e->serialize();
  }

}


