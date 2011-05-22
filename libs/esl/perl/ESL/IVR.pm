package ESL::IVR;
use ESL;
use Data::Dumper;

sub new($$) {
  my $proto = shift;
  my $class = ref($proto) || $proto;
  my $self = {};
  
  $self->{_esl} = new ESL::ESLconnection(fileno(STDIN));
  $self->{_info} = $self->{_esl}->getInfo();
  $self->{_uuid} = $self->{_info}->getHeader("unique-id");

  return bless($self, $class);

}

sub getHeader($;) {
  my $self = shift;

  return $self->{_info} ? $self->{_info}->getHeader(shift) : undef;
}

sub execute($;) {
  my $self = shift;
  return $self->{_esl}->execute(@_);
}

sub api($;) {
  my $self = shift;
  return $self->{_esl}->api(@_);
}

sub disconnect($;) {
  my $self = shift;
  return $self->{_esl}->disconnect(@_);
}

sub getVar($;) {
  my $self = shift;
  my ($var) = @_;
  my $e = $self->api("uuid_getvar", "$self->{_uuid} $var");
  my $input;

  if ($e) {
    $input = $e->getBody();
    if ($input && $input eq "_undef_") {
      $input = undef;
    }
  }
  
  return $input;  

}

sub setVar($;) {
  my $self = shift;
  my ($var, $val) = @_;
  my $e = $self->api("uuid_setvar", "$self->{_uuid} $var $val");
  my $input;
  if ($e) {
    $input = $e->getBody() . "\n";
    if ($input eq "_undef_") {
      $input = undef;
    }
  }

  chomp $input;

  return $input;

}

sub playAndGetDigits($;) {
  my $self = shift;
  my ($min, $max, $tries, $to, $term, $file, $invalid_file, $var, $regex, $digit_timeout) = @_;

  if (!$self->{_esl}->connected()) {
    return undef;
  }
  
  $self->execute("play_and_get_digits", "$min $max $tries $to $term $file $invalid_file $var $regex $digit_timeout");

  return $self->getVar($var);

}

sub read($;) {
  my $self = shift;
  my ($min, $max, $file, $var, $to, $term) = @_;
  
  if (!$self->{_esl}->connected()) {
    return undef;
  }

  $self->execute("read", "$min $max $file $var $to $term");

  return $self->getVar($var);

}

sub playback($;) {
  my $self = shift;
  my ($file) = @_;

  if (!$self->{_esl}->connected()) {
    return undef;
  }

  $self->execute("playback", $file);
  return $self->getVar("playback_terminators_used");
  
}


1;
