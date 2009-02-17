require ESL;

my $command = join(" ", @ARGV);
my $con = new ESL::ESLconnection("localhost", "8021", "ClueCon");
my $e = $con->sendRecv("api $command");
print $e->getBody();
