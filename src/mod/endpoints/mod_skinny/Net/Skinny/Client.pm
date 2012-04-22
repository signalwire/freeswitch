# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

package Net::Skinny::Client;

use strict;
use warnings;

use Config;
use threads;
use threads::shared;
use Thread::Queue;

require Net::Skinny;
use Net::Skinny::Protocol qw/:all/;
use Net::Skinny::Message;

our(@ISA);
@ISA = qw(Net::Skinny);

my $keep_alive_thread;
my $keep_alives :shared;
our $kept_self;
my $messages_send_queue;
my $messages_receive_queue;

$Config{useithreads} or die('Recompile Perl with threads to run this program.');

sub new {
	$kept_self = shift->SUPER::new(@_);
	$messages_send_queue = Thread::Queue->new();
	$messages_receive_queue = Thread::Queue->new();
	if ($kept_self) {
		threads->create(\&send_messages_thread_func);
		threads->create(\&receive_messages_thread_func);
	}
	return $kept_self;
}

sub send_message {
	my $self = shift;
	$messages_send_queue->enqueue(\@_);
}

sub receive_message {
	my $self = shift;
	my $message = $messages_receive_queue->dequeue();
	if($message->type() == 0x100) {#keepaliveack
		if(1) {
			lock($keep_alives);
			$keep_alives--;
		}
		$message = $messages_receive_queue->dequeue();
	}
	return $message;
}

sub launch_keep_alive_thread
{
	if(!$keep_alive_thread) {
		$keep_alive_thread = threads->create(\&keep_alive_thread_func);
	} else {
		print "keep-alive thread is already running\n";
	}
	return $keep_alive_thread;
}

sub keep_alive_thread_func
{
	while($kept_self) {
		if(1) {
			lock($keep_alives);
			$keep_alives++;
			$kept_self->send_message(KEEP_ALIVE_MESSAGE);
		} #mutex unlocked
		$kept_self->sleep(30, quiet => 0);
	}
}

sub send_messages_thread_func
{
	while(my $message = $messages_send_queue->dequeue()) {
		my $type = shift @$message;
		$kept_self->SUPER::send_message($type, @$message);
	}
}

sub receive_messages_thread_func
{
	while(1) {
		$messages_receive_queue->enqueue($kept_self->SUPER::receive_message());
	}
}

1;
