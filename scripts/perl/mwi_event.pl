# This is an example of sending an event via perlrun from the cli
# Edit to your liking.  perlrun mwi_event.pl
freeswitch::console_log("info", "Perl in da house!!!\n");

$event = new freeswitch::Event("message_waiting");
$event->add_header("MWI-Messages-Waiting", "yes");
$event->add_header("MWI-Message-Account", 'sip:1002@10.0.1.100');
$event->fire();
