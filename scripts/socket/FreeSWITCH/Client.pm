package FreeSWITCH::Client;
$|=1;
use IO::Socket::INET;
use IO::Select;
use Data::Dumper;

$VERSION = "1.0";

sub init($;$) {
  my $proto = shift;
  my $args = shift;
  my $class = ref($proto) || $proto;
  $self->{_host} = $args->{-host} || "localhost";
  $self->{_port} = $args->{-port} || 8021;
  $self->{_password} = $args->{-password} || undef; 
  $self->{_tolerant} = $args->{-tolerant} || undef;

  $self->{events} = [];
  my $me = bless $self,$class;
  if (!$self->{_password}) {
    return $me;
  }
  if ($me->connect()) {
    return $me;
  } else {
    return undef;
  }
}

sub readhash($;$) {
  my ($self,$to) = @_;
  my ($can_read) = IO::Select::select($self->{_sel}, undef, undef, $to);
  my $s = shift @{$can_read};
  my @r = ();
  my $crc = 0;
  my $h;

  if ($s) {
    for (;;) {
      my $line;
      for (;;) {
	my $i = 0;
	recv $s, $i, 1, 0;
	if ($i eq "") {	  
	  $h->{socketerror} = "yes";
	  return $h;
	} elsif ($i eq "\n") {
	  $crc++;
	  last;
	} else {
	  $crc = 0;
	}
	$line .= $i;      
      }

      if (!$line) {
	last;
      }
      push @r, $line;
    }
    
    if (!@r) {
      return undef;
    }

    foreach(@r) {
      my ($var, $val) = /^([^:]+):[\s\t]*(.*)$/;
      $h->{lc $var} = $val;
    }
    
    if ($h->{'content-length'}) {
      if(! defined $h->{body}) { $h->{body} = ""; }
      while(length($h->{body}) < $h->{'content-length'}) {
	my $buf;
	recv $s, $buf, $h->{'content-length'} - length($h->{body}), 0;
	if ($buf eq '') {
	  $h->{socketerror} = "yes";
	  return $h;	  
	}
	$h->{body} .= $buf;
      }
    }

    if ($h->{'content-type'} eq "text/event-plain") {
      my $e = $self->extract_event($h);
      $h->{has_event} = 1;
      $h->{event} = $e;
    }    
  }
    


  return $h;

}

sub error($$) {
  my($self,$error) = @_;

  if ($self->{"_tolerant"}) {
      print "[DIE CROAKED] $error\n";
      return 0;
  }
  else {
      die $error;
  }
}


sub output($$) {
  my ($self,$data) = @_;
  my $s = $self->{_sock};

  print $s $data ;
}

sub get_events($) {
  my $self = shift;
  my $e = $self->{events};
  $self->{events} = [];
  return $e;
}

sub sendmsg($$$) {
  my $self = shift;
  my $sendmsg = shift;
  my $to = shift;
  my $e;

  for(;;) {
    $e = $self->readhash(.1);
    if ($e && !$e->{socketerror}) {
      push @{$self->{events}}, $e;
    } else  {
      last;
    }
  }

  $self->output($sendmsg->{command} . "\n");
  foreach(keys %{$sendmsg}) {
    next if ($_ eq "command");
    $self->output("$_" . ": " . $sendmsg->{$_} . "\n");
  }
  $self->output("\n");

  return $self->readhash($to);
}

sub command($$) {
  my $self = shift;
  my $reply;

  my $r = $self->sendmsg({ 'command' => "api " . shift });

  if ($r->{body} ne '') {
    $reply = $r->{body};
  } elsif ($r->{'reply-text'} ne '') {
    $reply = $r->{'reply-text'};
  } else {
    $reply = "socketerror";
  }

  return $reply;
}

sub disconnect($) {
  my $self = shift;
  if ($self->{_sock}) {
    $self->{_sock}->shutdown(2);
    $self->{_sock}->close();
  }
  undef $self->{_sock};
  delete $self->{_sock};
}

sub raw_command($) {
  my $self = shift;
  return $self->sendmsg({ 'command' => shift });
}

sub htdecode($;$) {
  my $urlin = shift;
  my $url = (ref $urlin) ? \$$urlin : \$urlin;
  $$url =~ s/%([0-9A-Z]{2})/chr hex $1/ieg;
  $$url;
}


sub extract_event($$) {
  my $self = shift;
  my $r = shift;


  my %h = $r->{body} =~ /^([^:]+)\s*:\s*([^\n]*)/mg;

  foreach (keys %h) {
    my $new = lc $_;
    if (!($new eq $_)) {
      # do not delete keys that were already lowercase
      $h{$new} = $h{$_};
      delete $h{$_};
    }
  }
  foreach(keys %h) {
    htdecode(\$h{$_});
  }
  return \%h;
}


sub call_command($$$) {
  my $self = shift;
  my $app = shift;
  my $arg = shift;

  my $hash = {
	      'command' => "sendmsg",
	      'call-command' => "execute",
	      'execute-app-name' => $app,
	      'execute-app-arg' => $arg 
	     };

  return $self->sendmsg($hash);
}

sub unicast($$$$$$) {
  my $self = shift;

  my $hash = {
	      'command' => "sendmsg",
	      'call-command' => "unicast",
	      'local_ip' => $_[0],
	      'local_port' => $_[1],
	      'remote_ip' => $_[2],
	      'remote_port' => $_[3],
	      'transport' => $_[4]
	     };

  return $self->sendmsg($hash);
}

sub call_data($) {
  my $self = shift;

  return $self->{call_data};
}

sub accept($;$$) {
  my $self = shift;
  my $ip = shift;
  my $port = shift || 8084;

  if (!$self->{_lsock}) {
    $self->{_lsock} = IO::Socket::INET->new(Listen => 10000,
					    LocalAddr => $ip,
					    LocalPort => $port,
					    Reuse => 1,
					    Proto     => "tcp") or return $self->error("Cannot listen");

  }
  
  $self->{_sock} = $self->{_lsock}->accept();
  $self->{_sock}->autoflush(1);
  $self->{_sel} = new IO::Select( $self->{_sock} );

  $self->{call_data} = $self->sendmsg({ 'command' => "connect"});
  foreach(keys %{$self->{call_data}}) {
    htdecode(\$self->{call_data}->{$_});
  }
  if ($self->{call_data} =~ /socketerror/) {
    return 0;
  }

  return 1;

};

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
    $h = $self->sendmsg({command => "auth $pass"});
  }

  if ($h->{'reply-text'} =~ "OK") {
    return 1;
  }

  return 0;
}

1;
