use ExtUtils::MakeMaker;
require 5.008;
WriteMakefile(
	NAME => 'FreeSWITCH::Client',
	VERSION_FROM => 'Client.pm',
	PREREQ_PM => {
		'IO::Socket::INET' => 1.16,
		'IO::Select' => 1.16,
	},
);
