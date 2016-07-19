#!/usr/bin/perl
# Simple Fax Test
#
# 
#
use CGI qw(:standard);
use ESL;
use Data::Dumper;
use Data::UUID;
use XML::Simple;

# Replace Your CID Here
my $cid_num = "1NXXNXXXXXX";

my $q = new CGI; 
my $c = new ESL::ESLconnection("127.0.0.1", "8021", "ClueCon");

my $action = $q->param('action');

if($action eq 'log') {
    my $uuid = $q->param('uuid');
    
    if($uuid =~ m/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/) {
	if(-e "/tmp/$uuid.log") {
	    print $q->header('text/plain');
	    open(LOG, "</tmp/$uuid.log");
	    while (<LOG>) { print $_; }
	    close(LOG);
	} else {
	    print $q->header();
	    if(check_call($uuid)) {
		my $pages = pages_sent($uuid);
		print $q->start_html(-title=> 'FreeSWITCH Fax Results',
				     -head =>meta({-http_equiv => 'Refresh',
						   -content => "10;fax.cgi?uuid=$uuid&action=log"})),
		font({-color=>'black', -face=>'Arial', -size=>'4'}),
		"$pages pages(s) sent , Waiting on fax to complete.  Please Wait! Page will reload again in 10 seconds.",br,br,
		end_html;
	    } else {
		print $q->start_html(-title=> 'FreeSWITCH Fax Failed'),
		font({-color=>'black', -face=>'Arial', -size=>'4'}),
		"Fax call appears to have failed.",br,br,
		end_html;
	    }
	}
    }
} elsif ($action eq 'fax') {
    print $q->header;
    
    my $file    = '/var/www/fax.tif';
    my $fax     = $q->param('fax');
    my $ecm     = $q->param('ecm')   || 'false';
    my $v17     = $q->param('v17')   || 'false';
    my $t38     = $q->param('t38')   || 'false';
    my $large   = $q->param('large') || 'false';
    my $gateway = $q->param('gateway');
    my $refresh = 10;
    my $ug    = new Data::UUID;
    my $buuid = $ug->create();
    my $uuid  = $ug->to_string( $buuid );
    
    $fax =~ s/\D+//g;
    
    if($fax =~ m/^(1[2-9]\d{2}[2-9]\d{6})$/) {
	
	if($large eq 'true') {
	    $file = '/var/www/fax_large.tif';
	    $refresh = 60;
	} 
	
	my $e   = $c->sendRecv("api bgapi originate {fax_ident='FreeSWITCH Test Fax',fax_header='FreeSWITCH Test Fax',api_hangup_hook='system /bin/grep $uuid  /usr/local/freeswitch/log/freeswitch.log > /tmp/$uuid.log',origination_uuid=$uuid,fax_disable_v17=$v17,fax_use_ecm=$ecm,origination_caller_id_number=$cid_num,fax_verbose=true,fax_enable_t38=$t38,ignore_early_media=true,fax_enable_t38_request=$t38,t38_passthru=false,absolute_codec_string=PCMU}sofia/gateway/$gateway/$fax &txfax($file)");
	my $res = $e->getBody();
	print $q->start_html(-title=> 'FreeSWITCH Fax Results',
			     -head =>meta({
				 -http_equiv => 'Refresh', 
				 -content => "$refresh;fax.cgi?uuid=$uuid&action=log"})),br
	    font({-color=>'black', -face=>'Arial', -size=>'4'}),
	    "API Results: $res",br,br
	    "Send 10 Pages: $large",br,
	    "Enable T.38: $t38",br,
	    "Enable ECM: $ecm",br,
	    "Disable V17: $v17",br,
	    "Via Gateway: $gateway", br,br,
	    "Fax is queued to $fax immediately and will not retry on failure.",br,br
	    "Your log UUID is $uuid, wait here the page will reload showing you the results once complete",br,br,
	    end_html;
    } else {
	print "Invalid Number 1NXXNXXXXXX Only!";
    }
} else {
    my @gateways = load_gateways();

    print $q->header;

    print $q->start_html(-title=> 'FreeSWITCH Test Fax'), start_form,
    img( {-src => "data:image/png;base64," . <DATA>  }),br,br,font({-color=>'black', -face=>'Arial', -size=>'4'}),
    "Call will be coming from $cid_num",br,br,
    "Customer Fax Number: ", textfield('fax'),br,
    br,"Fax options:",br,hidden('action', 'fax'),
    br,checkbox(-label => 'Send 10 Pages', -name => "large", -value => 'true',  -selected => 0), br,
    br,checkbox(-label => 'Enable T.38',   -name => "t38",   -value => 'true',  -selected => 1), br,
    br,checkbox(-label => 'Enable ECM',    -name => "ecm",   -value => 'true',  -selected => 1), br,
    br,checkbox(-label => 'Disable v.17',  -name => "v17",   -value => 'true',  -selected => 0), br,
    br,'Using Gateway:',popup_menu(        -name=>'gateway', -values => \@gateways),br,
    br,submit('SEND FAX'),end_form,end_html;
}

sub check_call {
    my $uuid = shift;
    my $e   = $c->api("uuid_getvar $uuid uuid");
    my $res = $e->getBody();
    if($res =~ m/No such channel/) {
	return 0;
    }
    return 1;
}

sub pages_sent {
    my $uuid = shift;
    my $e   = $c->api("uuid_getvar $uuid fax_document_transferred_pages");
    my $res = $e->getBody();
    if ($res =~ /_undef_/) { return 0 };
    return $res;
}

# Query FreeSWITCH for gateway list to populate the test rig.
sub load_gateways {
    my $e     = $c->api('sofia xmlstatus gateways');
    my $gwxml = $e->getBody();
    
    my $ref = XMLin($gwxml);
    my @gateways;
    
    foreach my $key (keys %{ $ref->{gateway} }) {
	push @gateways, $key;
    }
    return @gateways;
}


__DATA__
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
