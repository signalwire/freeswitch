package FreeSWITCH::Client;
$|=1;
use IO::Socket::INET;
use IO::Select;
use Data::Dumper;



sub init($;$) {
  my $proto = shift;
  my $args = shift;
  my $class = ref($proto) || $proto;
  $self->{_host} = $args->{-host} || "localhost";
  $self->{_port} = $args->{-port} || 8021;
  $self->{_password} = $args->{-password} || undef; 

  my $me = bless $self,$class;
  if ($me->connect()) {
    return $me;
  } else {
    return undef;
  }
}

sub input($;$) {
  my ($self,$to) = @_;
  my $i;
  my @r;
  my $s = $self->{_sock};
  my $x = 0;
  my $done = 0;
  my $start = time;

  while(!$done) {
    if ($to and time - $start > $to) {
      last;
    }
    @ready = $self->{_sel}->can_read($to);
    if (@ready) {
      $x=0;
      foreach my $s (@ready) {
	while ($i = <$s>) {
	  $x++;
	  return @r if($i eq "\n");
	  $i =~ s/[\n]+$//g;
	  push @r,$i;
	  
	}
	unless($x) {
	  return ("SocketError: yes");
	}
      }
    }
  }
  return @r;

}

sub readhash($$) {
  my $self = shift;
  my $arg = shift;

  my @r = $self->input($arg);

  my $data = join "\n", @r;
  my %h = $data =~ /^([^:]+)\s*:\s*([^\n]*)/mg;

  foreach (keys %h) {
    my $new = lc $_;
    $h{$new} = $h{$_};
    delete $h{$_};
  }

  if ($h{'content-length'}) {
    my $s = $self->{_sock};
    read $s, $h{body}, $h{'content-length'};
  }

  return \%h;
}

sub error($$) {
  my($self,$error) = @_;
  die $error;
}


sub output($$) {
  my ($self,$data) = @_;
  my $s = $self->{_sock};

  print $s $data;
}

sub cmd($$$) {
  my $self = shift;
  my $cmd = shift;
  my $to = shift;

  $self->output($cmd);
  my $h = $self->readhash($to);

  $h;
}

sub connect($) {
  my $self = shift;

  $self->{_sock} = new IO::Socket::INET( Proto => 'tcp',
					 PeerAddr => $self->{_host},
					 PeerPort => $self->{_port}
				       ) or return $self->error("Connection refused $self->{_host} port $self->{_port}");

  $self->{_sock}->autoflush(1);
  #$self->{_sock}->blocking(0);
  $self->{_sel} = new IO::Select( $self->{_sock} );


  my $h = $self->readhash(undef);

  if ($h->{"content-type"} eq "auth/request") {
    my $pass = $self->{"_password"};
    $self->output("auth $pass");
    $h = $self->readhash(undef);
  }

  if ($h->{'reply-text'} =~ "OK") {
    return 1;
  }

  return 0;
}

1;
