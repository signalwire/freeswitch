require ESL;
use IO::Socket::INET;

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

  print $info->serialize();

  my $uuid = $info->getHeader("unique-id");

  $con->execute("answer", "", $uuid);
  $con->execute("playback", "/ram/swimp.raw", $uuid);

  close($new_sock);
}


