###############################################################################
#
#  This Perl module provides a "packet socket" of the kind that
#  XML-RPC For C/C++ uses for its "packet stream" variation on XML-RPC.
#
#  This module does not use the XML-RPC For C/C++ libraries.  It is 
#  pure Perl and layers on top of IO::Socket.
#
#  By Bryan Henderson, San Jose CA 08.03.12.
#
#  Contributed to the public domain by author.
#
###############################################################################

package IO::PacketSocket;

use strict;
use warnings;
use Exporter;
use Carp;
use vars qw(@ISA $VERSION @EXPORT);
use Errno qw(:POSIX);
use English;
use IO::Socket::INET


$VERSION = 1.00;
@ISA = qw(Exporter IO);

my ($TRUE, $FALSE) = (1,0); 

my $ESC = chr(0x1B);  # ASCII Escape

my $startDelim = $ESC . 'PKT';
my $endDelim   = $ESC . 'END';
my $escapedEsc = $ESC . 'ESC';


sub createObject {
    my ($class, %args) = @_;

    my $errorRet;
        # Description of why we can't create the object.  Undefined if
        # we haven't given up yet.

    my $packetSocket;

    $packetSocket = {};

    bless ($packetSocket, $class);

    if (defined($args{STREAMSOCKET})) {
        $packetSocket->{STREAMSOCKET} = $args{STREAMSOCKET};
    } else {
        $errorRet = "You must specify STREAMSOCKET";
    }

    $packetSocket->{RECEIVE_BUFFER} = '';
        
    if ($errorRet && !$args{ERROR}) {
        carp("Failed to create PacketSocket object.  $errorRet");
    }
    if ($args{ERROR}) {
        $ {$args{ERROR}} = $errorRet;
    }
    if ($args{HANDLE}) {
        $ {$args{HANDLE}} = $packetSocket;
    }
}



sub new {

    my ($class, %args) = @_;

    $args{HANDLE} = \my $retval;
    $args{ERROR}  = undef;

    $class->createObject(%args);

    return $retval;
}



sub escaped($) {
    my ($x) = @_;
#-----------------------------------------------------------------------------
#  Return $x, but properly escaped to be inside a packet socket
#  packet.
#-----------------------------------------------------------------------------

    $x =~ s{$ESC}{$escapedEsc}g;

    return $x;
}



sub unescaped($) {
    my ($x) = @_;
#-----------------------------------------------------------------------------
#  Inverse of escaped()
#-----------------------------------------------------------------------------

    $x =~ s{$escapedEsc}{$ESC}g;

    return $x;
}



sub send() {
    my($this, $payload) = @_;

    my $retval;

    my $packet = $startDelim . escaped($payload) . $endDelim;

    $retval = $this->{STREAMSOCKET}->send($packet);

    return $retval;
}



sub havePacket() {

    my ($this) = @_;

    return ($this->{RECEIVE_BUFFER} =~ m{$endDelim});
}



sub validatePacketStart($) {

    my ($packetR) = @_;

    my $delim = substr($$packetR, 0, 4);

    if ($startDelim !~ m{^$delim}) {
        die("Received bytes '$delim' are not in any packet.  " .
            "Sender is probably not using a packet socket");
    }
}



sub recv() {
    my ($this, $payloadR) = @_;

    my $gotPacket;
    my $eof;
    my $escapedPacket;

    $gotPacket = $FALSE;
    $eof = $FALSE;

    while (!$gotPacket && !$eof) {
        validatePacketStart(\$this->{RECEIVE_BUFFER});

        $this->{STREAMSOCKET}->recv(my $buffer, 4096, 0);

        if ($buffer eq '') {
            $eof = $TRUE;
        } else {
            $this->{RECEIVE_BUFFER} .= $buffer;
        }

        validatePacketStart(\$this->{RECEIVE_BUFFER});

        if ($this->{RECEIVE_BUFFER} =~
            m{^($startDelim)(.*?)($endDelim)(.*)}s) {

            ($escapedPacket, $this->{RECEIVE_BUFFER}) = ($2, $3);

            $gotPacket = $TRUE;
        }
    }

    $$payloadR = $eof ? '' : unescaped($escapedPacket);
}

1;
