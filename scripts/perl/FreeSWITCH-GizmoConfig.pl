#!/usr/bin/perl

# Simple script to add XML configuration for FreeSWITCH to use Gizmo5's phone network.
# This software has no support, warranty or liability: use at your own risk.
# Run without arguments for help.

use strict;

sub main () {

	my ($command) = parseArgs();
	my ($err) = checkArgs($command);

	if ($err) {

		usage($err);
		exit(1);

	}


	if ($command->{action}  && $command->{action} =~ /^add/i) {

		if ($command->{object} && $command->{object} =~ /^gateway/i) {

			($err) = createGizmo5Gateway($command) unless ($err);
			($err) = createGizmo5DialPlan($command) unless ($err);

			print "All files are in place.  You must restart FreeSWITCH or run the console command: xml_reload\n" unless ($err);
			print "for any changes to take effect.\n" unless ($err);
			
		} elsif ($command->{object} && $command->{object} =~ /^callin/i) {

			($err) = createGizmo5Callin($command) unless ($err);

			print "All files are in place.  You must restart FreeSWITCH or run the console command: xml_reload\n" unless ($err);
			print "for any changes to take effect.\n" unless ($err);

		}


	### No support to remove yet.
	} elsif ($command->{action} && $command->{action} =~ /^del/i) {

		if ($command->{object} && $command->{object} =~ /^gateway/i) {

		} elsif ($command->{object} && $command->{object} =~ /^callin/i) {

		}

	}

	if ($err) {

		print "Error: $err\n";
		exit(1);

	}

	exit(0);

}

sub createGizmo5Callin($) {

	my ($command) = @_;

	my $template = <<"END_O_FILE";
<include>
  <extension name="Gizmo5 DID SIP_DID">
    <condition field="destination_number" expression="SIP_DID">
      <action application="transfer" data="SIP_LOCALUSER XML default"/>
    </condition>
  </extension>
</include>

END_O_FILE
	
	$template =~ s/SIP_LOCALUSER/$command->{extension}/g;
	$template =~ s/SIP_DID/$command->{did}/g;

	return("Gizmo callin for $command->{did} appears to already exist at $command->{prefix}/conf/dialplan/public/01_gizmo-$command->{did}.xml") if (-e "$command->{prefix}/conf/dialplan/public/01_gizmo-$command->{did}.xml");

	open(DEFAULT,">$command->{prefix}/conf/dialplan/public/01_gizmo-$command->{did}.xml") || return ("Unable to open $command->{prefix}/conf/dialplan/public/01_gizmo-$command->{did}.xml"); 
	print DEFAULT $template;
	close (DEFAULT);

	return(); 

}

sub createGizmo5DialPlan($) {

	my ($command) = @_;
	my $template = <<"END_O_FILE";
<!-- Gizmo5 default "catchall" dialplan extension. -->
<!-- Calls to all Gizmo5 users and SIP domains are free. -->
<!-- Callout to PSTN requires an account with a balance. -->
<!-- Visit https://my.gizmo5.com to top-up your accounts.  -->

<include>   
  <extension name="Gizmo5 Default Gateway">
    <condition field="destination_number" expression="^(.+)\$">
      <action application="bridge" data="sofia/gateway/$command->{username}proxy01.sipphone.com/\$1"/>
    </condition>
  </extension>
<include>

END_O_FILE

	return ("Gizmo Default callout appears to already exist at $command->{prefix}/conf/dialplan/default/01_gizmo-$command->{username}.xml") if (-e "$command->{prefix}/conf/dialplan/default/01_gizmo-$command->{username}.xml");

	open(DEFAULT,">$command->{prefix}/conf/dialplan/default/01_gizmo-$command->{username}.xml") || return("Unable to open $command->{prefix}/conf/dialplan/default/01_gizmo-$command->{username}.xml");
	print DEFAULT $template;
	close (DEFAULT);


	$template = <<"END_O_FILE";
<!-- Gizmo5 default SIP Number -->
<!-- This allows you to route incoming calls from your Gizmo5 account --> 
<!-- to extension SIP_LOCALUSER. -->
<!-- You can use more logic to route from here. -->

<include>
  <extension name="Gizmo5 Default Callin">
    <condition field="destination_number" expression="SIP_DID">
      <action application="transfer" data="SIP_LOCALUSER XML default"/>
    </condition>
  </extension>
</include>

END_O_FILE

	$template =~ s/SIP_DID/$command->{username}/g;
	$template =~ s/SIP_LOCALUSER/$command->{extension}/g;

	return("Gizmo Default callin appears to already exist at $command->{prefix}/conf/dialplan/public/01_gizmo-$command->{username}.xml") if (-e "$command->{prefix}/conf/dialplan/public/01_gizmo-$command->{username}.xml");

	open(DEFAULT,">>$command->{prefix}/conf/dialplan/public/01_gizmo-$command->{username}.xml") || return("Unable to open $command->{prefix}/conf/dialplan/public/01_gizmo-$command->{username}.xml");
	print DEFAULT $template;
	close (DEFAULT);
	
	return();
}

sub createGizmo5Gateway($) {

	my ($command) = @_;

	my $template = <<"END_O_FILE";
<!-- Gizmo5 SIP Registration -->
<!-- This is required for FreeSWITCH to work with Gizmo5 -->

<include>
  <user id="\$\${default_provider}\">
    <gateways>
      <gateway name="SIP_USERproxy01.sipphone.com">
        <param name="username" value="SIP_USER"/>
        <param name="from-domain" value="proxy01.sipphone.com"/>
        <param name="password" value="SIP_PASS"/>
        <param name="caller-id-in-from" value="false"/>
        <param name="proxy" value="proxy01.sipphone.com"/>
        <param name="expire-seconds" value="3600"/>
        <param name="register-transport" value="tcp"/>
        <param name="register" value="true"/>
        <param name="retry_seconds" value="3600"/>
      </gateway>
    </gateways>
  </user>
</include>

END_O_FILE

	$template =~ s/SIP_USER/$command->{username}/g;
	$template =~ s/SIP_PASS/$command->{password}/;

	return "Gizmo Gateway appears to already exist at $command->{prefix}/conf/directory/default/01_gizmo-$command->{username}.xml" if (-e "$command->{prefix}/conf/directory/default/01_gizmo-$command->{username}.xml"); 

	open(GIZMO,">$command->{prefix}/conf/directory/default/01_gizmo-$command->{username}.xml") || return ("Unable to open $command->{prefix}/conf/directory/default/01_gizmo-$command->{username}.xml to write");
	print GIZMO $template;
	close(GIZMO);

	return();
	
}

sub checkArgs($) {

	my ($command) = @_;
	my ($err) = 0;

	return("Bad Action") if (!defined $command->{action} || $command->{action} !~ /^(add)/);
	return("Bad Object") if (!defined $command->{object} || $command->{object} !~ /^(gateway|callin)$/i); 
	return("Bad Username") if ($command->{action} && $command->{action} =~ /^add/i && (!defined $command->{username} || $command->{username} !~ /^1747\d{7}$/));
	return("Bad Extension") if ($command->{action} && $command->{action} =~ /^add/i && (!defined $command->{extension} || $command->{extension} eq '' ));
	return("No Password Given") if ($command->{action} && $command->{action} =~ /^add/i && $command->{object} && $command->{object} =~ /^gateway/i && (!defined $command->{password} || $command->{password} eq ''));
	return("No Local Extension Given") if ($command->{action} && $command->{action} =~ /^add/i && $command->{object} && $command->{object} =~ /^callin/i && (!defined $command->{extension} || $command->{extension} eq ''));
	return("DID must be purely numeric") if ($command->{action} && $command->{action} =~ /^add/i && $command->{object} && $command->{object} =~ /^callin/i && (!defined $command->{did} || $command->{did} !~ /^\d+$/));


	return("Cannot find FreeSWITCH install at $command->{prefix}") unless (-e "$command->{prefix}/conf" && -d "$command->{prefix}/conf");
	return("Cannot write to FreeSWITCH install at $command->{prefix}") unless (-w "$command->{prefix}/conf");

	return();

}

sub parseArgs() {

	my $command;

	foreach (@ARGV) {

		if (/^(.*)=(.*)/) {

			$command->{lc($1)} = $2;

		}

	}

	$command->{action} = $ARGV[0] || undef;
	$command->{object} = $ARGV[1] || undef;
	$command->{prefix} = '/usr/local/freeswitch' unless ($command->{prefix} && $command->{prefix} ne '');
	return ($command);

}


sub usage($) {

	my ($err) = @_;

	print STDERR "$err\n" if ($err);
	print STDERR "Usage:  ./FreeSWITCH-GizmoConfig.pl action object [arguments [...]]\n";
	print STDERR "Valid actions:\n";
	print STDERR "add gateway username=1747NNNNNNN password=xxxxxx extension=YourExtension [prefix=/usr/local/freeswitch]\n";
	print STDERR "add callin did=NNNNNNNNN extension=YourExension [prefix=/usr/local/freeswitch]\n";
	print STDERR "\n";
	print STDERR "Example of adding user 17472075873:\n";
	print STDERR "./FreeSWITCH-Gizmo.pl add gateway username=17472075873 password=Secret extension=1001\n";
	print STDERR "\n";
	print STDERR "Adding support for a US callin Number: +1-858-625-0499 (You must add a user that purchased the number to receive calls):\n";
	print STDERR "./FreeSWITCH-Gizmo.pl add callin did=18586250499 extension=1001\n";
	print STDERR "\n";
	print STDERR "Adding support for a UK or other International callin Number: +44 20 7499-9000 :\n";
	print STDERR "./FreeSWITCH-Gizmo.pl add callin did=442074999000 extension=brian\n";

}

main();

