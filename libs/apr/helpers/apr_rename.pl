#!/usr/bin/perl -w
use strict;
use ExtUtils::MakeMaker qw(prompt);
use File::Find;

my $just_check = @ARGV ? $ARGV[0] eq '-c' : 0;
shift if $just_check;
my $dir = shift || '.';
my %names;

my $prefix = 'fspr_';

while (<DATA>) {
    chomp;
    my($old, $new) = grep { s/^$prefix//o } split;
    next unless $old and $new;
    $names{$old} = $new;
}

my $pattern = join '|', keys %names;
#print "replacement pattern=$pattern\n";

find sub {
    chomp;
    return unless /\.[ch]$/;
    my $file = "$File::Find::dir/$_";
    print "looking in $file\n";

    replace($_, !$just_check);

}, $dir;

sub replace {
    my($file, $replace) = @_;
    local *IN, *OUT;
    my @lines;
    my $found = 0;

    open IN, $file or die "open $file: $!";

    while (<IN>) {
        for (m/[^_\"]*$prefix($pattern)\b/og) {
            $found++;
            print "   $file:$. fspr_$_ -> fspr_$names{$_}\n";
        }
        push @lines, $_ if $replace;
    }

    close IN;

    return unless $found and $replace;

#    my $ans = prompt("replace?", 'y');
#    return unless $ans =~ /^y/i;

    open OUT, ">$file" or die "open $file: $!";

    for (@lines) {
        unless (/^\#include/) {
            s/([^_\"]*$prefix)($pattern)\b/$1$names{$2}/og;
        }
        print OUT $_;
    }

    close OUT;
}

__DATA__
fspr_time_t:
fspr_implode_gmt              fspr_time_exp_gmt_get

fspr_socket_t:
fspr_close_socket             fspr_socket_close
fspr_create_socket            fspr_socket_create
fspr_get_sockaddr             fspr_socket_addr_get
fspr_get_socketdata           fspr_socket_data_get
fspr_set_socketdata           fspr_socket_data_set
fspr_shutdown                 fspr_socket_shutdown
fspr_bind                     fspr_socket_bind
fspr_listen                   fspr_socket_listen
fspr_accept                   fspr_socket_accept
fspr_connect                  fspr_socket_connect
fspr_send                     fspr_socket_send
fspr_sendv                    fspr_socket_sendv
fspr_sendto                   fspr_socket_sendto
fspr_recvfrom                 fspr_socket_recvfrom
fspr_sendfile                 fspr_socket_sendfile
fspr_recv                     fspr_socket_recv

fspr_filepath_*:
fspr_filename_of_pathname     fspr_filepath_name_get

fspr_gid_t:
fspr_get_groupid              fspr_gid_get
fspr_get_groupname            fspr_gid_name_get
fspr_group_name_get           fspr_gid_name_get
fspr_compare_groups           fspr_gid_compare

fspr_uid_t:
fspr_get_home_directory       fspr_uid_homepath_get
fspr_get_userid               fspr_uid_get
fspr_current_userid           fspr_uid_current
fspr_compare_users            fspr_uid_compare
fspr_get_username             fspr_uid_name_get
fspr_compare_users            fspr_uid_compare

