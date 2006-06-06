#!/usr/local/bin/perl
##########################################################################
#  rss2ivr.pl -- A Script to turn an RSS feed into audio files.
#
#  Copyright (C) 2006, Anthony Minessale
#
#  Anthony Minessale <anthmct@yahoo.com>
#
#  This program is free software, distributed under the terms of
#  Perl itself
##########################################################################
use XML::RSS;
require LWP::UserAgent;
################################################################################
my $swift = "/opt/swift/bin/swift";
################################################################################

my $rss = new XML::RSS;
my $root = shift;
my $target = shift;
my $dir = shift;
my $saytxt;
my $voice = shift || "David";
my $ua = LWP::UserAgent->new;
$ua->timeout(10);

unless($root and $target and $dir) {
  die("Usage: $0 <root dir> <target url> <ivr name>\n");
}

unless(-d $root) {
    mkdir($root);
}

chdir($root) or die("where is $root");

if ($target =~ /http/) {
    my $response = $ua->get($target);
    if ($response->is_success) {
	$in = $response->content;
    } else {
	die $response->status_line;
    }

    my $x = 1;

    $rss->parse($in);
    system("rm -fr $$");
    mkdir($$);
    die unless(-d $$);
    chdir($$);
    open(I,">00.txt");
    print I "$dir main index.\n";

    foreach my $item (@{$rss->{'items'}}) {
	my $xx = sprintf("%0.2d", $x);
	my $title .= "entry $xx, " . $item->{'title'} . ".\n";
	print I $title;
	my $desc = $item->{'description'};
	$desc =~ s/\<[^\>]+\>//g;
	my $content = "entry $xx.\n" . $item->{'title'} . ".\n". $desc;    
	open(X,">$xx.txt");
	$content =~ s/[^\w\d\s \.\,\-\n]//smg;

	print X $content;
	close X;

	my $do = "$swift -p audio/deadair=2000,audio/sampling-rate=8000,audio/channels=1,audio/encoding=pcm16,audio/output-format=raw -o ${xx}.raw -f ${xx}.txt -n $voice";
	system $do;
	$x++;
    }
    
    my $do = "$swift -p audio/deadair=2000,audio/sampling-rate=8000,audio/channels=1,audio/encoding=pcm16,audio/output-format=raw -o 00.raw -f 00.txt -n $voice";
    system $do;
    close(I);
    chdir("..");
    system("/bin/rm -fr $dir");
    system("/bin/mv -f $$ $dir");
}

