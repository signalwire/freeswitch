require ESL;

my $command = shift;
my $args = join(" ", @ARGV);

my $con = new ESL::ESLconnection("localhost", "8021", "ClueCon");
my $e = $con->bgapi($command, $args);
print $e->getBody();
