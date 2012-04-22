#!/usr/bin/perl
use Frontier::Responder;
use Data::Dumper;
require ESL;
#For use with FreeSWITCH Dialer.scpt in applescripts
#
# Install FreeSWITCH Dialer.scpt into ~/Library/Address Book Plug-Ins/
#


sub MakeCall {
    my $hash = shift;
    my $c = new ESL::ESLconnection("localhost", "8021", "ClueCon");
    my $number = $hash->{phoneNumber};
    my $user = $hash->{userExtension};
    my $gateway = $hash->{useGateway};
    $number =~ s/\D//g; # Remove nasties.
    my $e = $c->sendRecv("bgapi originate {ignore_early_media=true,origination_caller_id_number=$number,effective_caller_id_number=19183029101}$user &bridge({ignore_early_media=false,sip_authorized=true}$gateway/$number)");
    $e->getBody();
}

my $res = Frontier::Responder->new(
				   methods => {
				       MakeCall => \&MakeCall,
				   },
				   );

print $res->answer;
