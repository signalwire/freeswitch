#!/usr/bin/perl
use strict;
use warnings;

sub POE::Kernel::ASSERT_DEFAULT () { 1 };
sub Term::Visual::DEBUG () { 1 }
sub Term::Visual::DEBUG_FILE () { 'test.log' }
use IO::Socket;
use POE qw/Filter::FSSocket Component::Client::TCP/;
use Data::Dumper;
use Term::Visual;


local *D;
if (Term::Visual::DEBUG) {
    *D = *Term::Visual::ERRS;
}

#local *ERROR = *STDERR;


$SIG{__DIE__} = sub {
    if (Term::Visual::DEBUG) {
        print Term::Visual::ERRS "Died: @_\n";
    }
};

###############################################################################
## BEGIN Globals ##############################################################
###############################################################################
our $server_address = "127.0.0.1";
our $server_port    = "8021";
our $server_secret  = "ClueCon";

#this is where you can customize the color scheme
our %Pallet = (
	'warn_bullet' => 'bold yellow',
	'err_bullet'  => 'bold red',
	'out_bullet'  => 'bold green',
	'access'      => 'bright red on blue',
	'current'     => 'bright yellow on blue',
);

our $terminal;
my %sockets;
my %windows;
my %unread_count;
my %commands = (
	'window' => 1,
	'w'      => 1,
	'win'    => 1,
);
###############################################################################
##   END Globals ##############################################################
###############################################################################

#setup our session
POE::Session->create(
	'inline_states' => {
		'_start'       => \&handle_start,        #session start
		'_stop'        => \&handle_stop,         #session stop
		'curses_input' => \&handle_curses_input, #input from the keyboard
		'update_time'  => \&handle_update_time,  #update the status line clock
		'quit'         => \&handle_quit,         #handler to do any cleanup
		'server_input' => \&handle_server_input,
		'_default'      => \&handle_default,
	},
	'heap' => {
		'terminal'     => undef,
		'freeswitch'   => undef,
	},
);

#start the kernel a chugging along
$poe_kernel->run;

###############################################################################
## BEGIN Handlers #############################################################
###############################################################################
#handles any startup functions for our session
sub handle_default {
}

sub handle_start {
	my ($kernel, $session, $heap) = @_[KERNEL, SESSION, HEAP];

	#setup our terminal
	 $heap->{'terminal'} = Term::Visual->new(
		'Alias'        => 'terminal', #poe alias for this
		'History_Size' => 300,        #number of things to keep in history
		'Common_Input' => 1,          #all windows share input and history
		'Tab_Complete' => \&tab_complete,
	);

	$terminal = $heap->{'terminal'};

	#setup the color palette
	$terminal->set_palette(%Pallet);

	#create a base window
	my $window_id = $terminal->create_window(
		'Window_Name' => 'console',
		'Buffer_Size' => 3000,
		'Title'       => 'FreeSWITCH Console',
		'Status'      => {
			'0' => {
				'format' => '%s',
				'fields' => ['time'],
			},
			'1' => {
				'format' => '%s',
				'fields' => ['window_status'],
			},
		},
	);

	$windows{'console'} = $window_id;

	$window_id = $terminal->create_window(
		'Window_Name' => 'log',
		'Buffer_Size' => 3000,
		'Title'       => 'FreeSWITCH Logs',
		'Status'      => {
			'0' => {
				'format' => '%s',
				'fields' => ['time'],
			},
			'1' => {
				'format' => '%s',
				'fields' => ['window_status'],
			},
		},
	);

	$windows{'log'} = $window_id;

	$window_id = $terminal->create_window(
		'Window_Name' => 'event',
		'Buffer_Size' => 3000,
		'Title'       => 'FreeSWITCH Event',
		'Status'      => {
			'0' => {
				'format' => '%s',
				'fields' => ['time'],
			},
			'1' => {
				'format' => '%s',
				'fields' => ['window_status'],
			},
		},
	);

	$windows{'event'} = $window_id;

	#tell the terminal what to call when there is input from the keyboard
	$kernel->post('terminal' => 'send_me_input' => 'curses_input');

	$terminal->change_window(0);
	$kernel->delay_set('update_time' => 1);
	$terminal->set_status_field(0, 'time' => scalar(localtime));
	new_message('destination_window' => 0, 'message' =>  "
Welcome to the FreeSWITCH POE Curses Console!
  The console is split into three windows:
    - 'console' for api response messages
    - 'log'     for freeswitch log output (simply send the log level you want 
                  to start seeing events eg: 'log all')
    - 'event'   for freeswitch event output (must subscribe in plain format
                  eg: 'event plain all')

To switch between windows type 'w <windowname' so 'w log' for example.

Coming soon:
  - Tab completion
  - command history
  - window status in the bar (messages added since last view, etc...)

Send any bug reports or comments to jackhammer\@gmail.com

Thanks,
Paul\n");

	$terminal->set_status_field($terminal->current_window, 'window_status' => format_window_status());

	#connect to freeswitch
	$heap->{'freeswitch'} = POE::Component::Client::TCP->new(
		'RemoteAddress' => $server_address,
		'RemotePort'    => $server_port,
		'ServerInput'   => \&handle_server_input,
		'Connected'     => \&handle_fs_connected,
		'ServerError'   => \&handle_server_error,
		'Disconnected'  => \&handle_server_disconnect,
		'Domain'        => AF_INET,
		'Filter'       => POE::Filter::FSSocket->new(),
	);

}

#called when users enter commands in a window
sub handle_curses_input {
	my ($kernel, $heap, $input, $context) = @_[KERNEL, HEAP, ARG0, ARG1];

	#get the id of the window that is responsible for the input
	my $window = $heap->{'terminal'}->current_window;

	open(ERROR, ">>error.log");

	if($input eq "quit") {
		$kernel->yield('quit');
	} elsif ($input =~ /^w\ (.*)$/) {
		#get the id of the requested window
		eval {
			my $window_id = $windows{$1};

			#see if it's real
			if(defined($window_id)) {
				$unread_count{$window_id} = 0;
				$terminal->change_window($window_id);
				$terminal->set_status_field($window_id, 'window_status' => &format_window_status());
			}
		};
		if($@) {
			print ERROR "put error: $@\n";
		}
	} else {
		#see if we got connected at some point
		if(defined($sockets{'localhost'})) {
		  my $cmd;
		  if ($input =~ /^log|^event/) {
		    $cmd = $input;
		  } else {
		    $cmd = "api $input";
		  }
		  #send the command
		  $sockets{'localhost'}->put($cmd);
		}
	}
}

sub handle_fs_connected {
	my ($kernel, $heap) = @_[KERNEL, HEAP];

	eval {
		$sockets{'localhost'} = $heap->{'server'};
	}
}

#this is responsible for doing any cleanup and returning the terminal to the previous
#state before we mucked with it
sub handle_quit {
	my ($kernel, $heap) = @_[KERNEL, HEAP];

	#tell curses to clean up it's crap
	$kernel->post('terminal' => 'shutdown');

	#there is probably a more elegant way, but this works for now
	exit;
}

#data from freeswitch
sub handle_server_input {
        my ($kernel,$heap,$input) = @_[KERNEL,HEAP,ARG0];

	eval {
		#terminal HATES null
		if(defined($input->{'__DATA__'})) {
			$input->{'__DATA__'} =~ s/[\x00]//g;
		}

		#handle the login
		if($input->{'Content-Type'} eq "auth/request") {
			$heap->{'server'}->put("auth $server_secret");
		} elsif ($input->{'Content-Type'} eq "api/response") {
			new_message('destination_window' => 0, 'message' => 'API Response: ');
			new_message('destination_window' => 0, 'message' => $input->{'__DATA__'});
		} elsif ($input->{'Content-Type'} eq "log/data") {
			new_message('destination_window' => 1, 'message' => $input->{'__DATA__'});
		} elsif ($input->{'Content-Type'} eq "text/event-plain") {
			new_message('destination_window' => 2, 'message' => Dumper $input);
		} elsif ($input->{'Content-Type'} eq "command/reply") {
			new_message('destination_window' => 0, 'message' => 'Command Response: ' . $input->{'Reply-Text'});
		}
	};

	if($@) {
		open(ERROR, ">>error.log");
		print ERROR "died: $@\n";
		print ERROR Dumper $heap;
		close(ERROR);
	}
}

sub handle_server_error {
}

sub handle_server_disconnect {
}

sub tab_complete {
	my $left = shift;

	my @return;

	if(defined($commands{$left})) {
		return [$left . " "];
	#} elsif () {
	}
		
}

sub handle_update_time {
	my ($kernel, $heap) = @_[KERNEL, HEAP];
	$terminal->set_status_field($terminal->current_window, 'time' => scalar(localtime));
	$kernel->delay_set('update_time' => 1);
}
###############################################################################
##   END Handlers #############################################################
###############################################################################

sub new_message {
	my %args = @_;

	my $message            = $args{'message'};
	my $destination_window = $args{'destination_window'};

	my $status_field;

	#see if we are on the window being updated
	if($terminal->current_window != $destination_window) {
		#increment the unread count for the window
		#FIXME, should we count messages or lines?
		$unread_count{$destination_window}++;


		#update the status bar
		eval {
			$terminal->set_status_field($terminal->current_window, 'window_status' => &format_window_status());
		};

		if($@) {
			print $@;
		}
	}


	#deliver the message
	$terminal->print($destination_window, $message);
}

sub format_window_status {
	my $status_field;

	#put all the windows in the bar with their current unread count
	foreach my $window (sort {$windows{$a} <=> $windows{$b}} keys %windows) {
		#see if we are printing the current window
		if($terminal->current_window == $windows{$window}) {
			$status_field .= "[\0(current)$window\0(st_frames)";
		} else {
			$status_field .= "[$window";
		}

		if($unread_count{$windows{$window}}) {
			$status_field .= " (" . $unread_count{$windows{$window}} . ")";
		}

		$status_field .= "] ";
	}

	return $status_field;
}
