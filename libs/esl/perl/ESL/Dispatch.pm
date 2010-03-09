package ESL::Dispatch;
use Data::Dumper;
require ESL;
require Exporter;
use AutoLoader ();

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $AUTOLOAD);

$VERSION = "1.0";

@ISA = qw(Exporter DynaLoader);


sub init($;$) {
  my $proto = shift;
  my $args = shift;
  my $class = ref($proto) || $proto;
  my $self = {};

  $self->{_debug} = $args->{debug} ||= 0;

  $self->{host} = $args->{host} ||= "localhost";
  $self->{port} = $args->{port} ||= "8021";
  $self->{pass} = $args->{pass} ||= "ClueCon";
  $self->{_esl} = new ESL::ESLconnection("$self->{host}", "$self->{port}", "$self->{pass}");
  $self->{_callback} = undef;
  $self->{_custom_subclass} = undef;
  return bless($self, $class);
}

sub set_worker($;$$) {
  my $self = shift;
  $self->{_worker} = shift;
  $self->{_timeout} = shift;
}

sub set_callback($;$$) {
  my $self = shift;
  my $event = shift;
  $self->{_callback}->{$event} = shift;
  my $subclass = shift;
  if($subclass) {
    $self->{_custom_subclass} = split(/,/, $subclass);
  }
}

sub render_event($;$) {
  my $self = shift;
  my $event = shift;
  my $h = $event->firstHeader();

  while ($h) {
    $val = $event->getHeader($h);
    if($self->{_debug} > 3) {
      print "$h -> $val\n";
    }
    if ($self->{_decode}) {
      $val =~ s/\%([A-Fa-f0-9]{2})/pack('C', hex($1))/seg;
    }
    $h =~ s/^variable_//;
    $self->{event_hash}->{lc($h)} = $val;
    $h = $event->nextHeader();
  }
  # Execute callback for this event
  eval {
    if($self->{_debug}) {
      $callback = lc($self->{event_hash}->{'event-name'});
      print "DEBUG: executing $callback callback\n";
    }
    &{$self->{_callback}->{lc($self->{event_hash}->{'event-name'})}}($self, $self->{event_hash});
  };
}


sub run($;) {
  my $self = shift;
  my $event;

  for(;;) {
    # Only register for events we have callbacks for.
    for my $key ( keys %{$self->{_callback}} ) {
      if ($key eq "CUSTOM") {
	foreach $subclass (@{$self->{_custom_subclass}}) {
	  $self->{_esl}->events("plain", "$key $subclass");
	}
	next;
      }
      $self->{_esl}->events("plain", "$key");
    }

    while ($self->{_esl}->connected()) {
      if($self->{_timeout} > 0) {
	$event = $self->{_esl}->recvEventTimed($self->{_timeout});
	if(!$event) {
	  eval {&{$self->{_worker}}($self);};
	  next;
	}
      } else {
	$event = $self->{_esl}->recvEvent();
      }
      $self->render_event($event,1);
      delete $self->{event_hash};
    }
    sleep 1;
    $self->{_esl} = new ESL::ESLconnection("$self->{host}", "$self->{port}", "$self->{pass}");
  }
}

1;
