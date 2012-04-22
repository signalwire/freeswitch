require ESL;
use IO::Socket::INET;

#ESL::eslSetLogLevel(7);

my $ip = "127.0.0.1";
my $sock = new IO::Socket::INET ( LocalHost => $ip,  LocalPort => '8040',  Proto => 'tcp',  Listen => 1,  Reuse => 1 );
die "Could not create socket: $!\n" unless $sock;

for(;;) {
  my $new_sock = $sock->accept();
  my $pid = fork();
  if ($pid) {
    close($new_sock);
    next;
  }

  my $host = $new_sock->sockhost();
  my $fd = fileno($new_sock);
  
  my $con = new ESL::ESLconnection($fd);
  my $info = $con->getInfo();

  #print $info->serialize();
  my $uuid = $info->getHeader("unique-id");

  printf "Connected call %s, from %s\n", $uuid, $info->getHeader("caller-caller-id-number");

  $con->sendRecv("myevents");
  $con->execute("answer");
  $con->sendRecv("divert_events on");
  $con->execute("detect_speech", "pocketsphinx yes_no yes_no");
  $con->execute("playback", "/ram/swimp.raw");
  
  while($con->connected()) {
    #my $e = $con->recvEventTimed(100);
    my $e = $con->recvEvent();
    
    if ($e) {
      my $name = $e->getHeader("event-name");
      print "EVENT [$name]\n";
      if ($name eq "DETECTED_SPEECH") {
	print $e->serialize();
	my $type = $e->getHeader("speech-type");
	if ($type = "detected-speech") {
	  $con->execute("detect_speech", "resume");
	}
      }
      if ($name eq "DTMF") {
	my $digit = $e->getHeader("dtmf-digit");
	my $duration = $e->getHeader("dtmf-duration");
	print "DTMF digit $digit ($duration)\n";
      }
    }
  }

  print "BYE\n";
  close($new_sock);
}


