use IO::Socket::Multicast;
my ($ip,$port) = @ARGV;

$ip and $port or die "Usage $0: <ip> <port>\n";

# create a new UDP socket and add a multicast group
my $socket = IO::Socket::Multicast->new( LocalPort => $port, ReuseAddr => 1 );
$socket->mcast_add($ip);

while($socket->recv($data,1024)) {
  print $data;
}
