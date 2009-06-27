#!/usr/bin/perl

use ExtUtils::MakeMaker;

WriteMakefile(
	NAME         => 'POE::Filter::FSSocket',
	AUTHOR       => 'Paul Tinsley <pdt@jackhammer.org>',
	ABSTRACT     => 'POE filter for getting events out of FreeSWITCH',
	VERSION_FROM => 'lib/POE/Filter/FSSocket.pm',

	PM           => { 'lib/POE/Filter/FSSocket.pm' => '$(INST_LIBDIR)/FSSocket.pm' },
	PREREQ_PM    => {
		POE        => 0.3101,
		Test::More => 0,
		POE::Component::Client::TCP => 0,
		POE::Filter::Line => 0,
	},
	dist         => {
		COMPRESS   => 'gzip -9f',
		SUFFIX     => 'gz',
	},
);
