#!/usr/bin/perl

sub fixTzstr {
    # switch_time.c expects POSIX-style TZ rule, but it won't process quoted TZ
    # rules that look like this: <-04>4 or <-04>4<-03>
    # See https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03

    # Instead it defaults to UTC for these values. Here we process the quoted
    # values and convert them into letters. If the zone name has "GMT", we use
    # that as the replacement prefix, otherwise a default "STD" is used. Zones
    # that have a quoted suffix have their suffix replaced with "DST".

    my ($tzstr, $name) = @_;

    if ( $tzstr =~ /(<(?<std>[^>]+)>)([^<]+)(?<dst><.+>)?(?<rest>.+)?/ ) {
        my ($tzprefix, $tzsuffix, $tzrest, $offset, $offsetprefix) = ("") x 5;

        if ( defined($+{std}) ) {
            my $std = $+{std};
            
            if ( lc($name) =~ m/gmt/) {
                $tzprefix = "GMT";
            } else {
                $tzprefix = "STD"; 
            }

            if ( $std =~ m/\+/ )  {
                $offset = sprintf "%d", $std =~ s/\+//r;
                $offsetprefix = "-";
            } else {
                $offset = sprintf "%d", $std =~ s/\-//r;
            }

            my @chars = split(//, $offset);
            if ( @chars > 2 ) {
                my $hours = $chars[-3];
                if ( defined( $chars[-4] ) ) {
                    $hours = $chars[-4].$hours;
                }

                $offset = $hours.":".$chars[-2].$chars[-1];
            }

            $offset = $offsetprefix.$offset;
        }

        if ( defined($+{dst}) ) {
            $tzsuffix = "DST";
        }

        if ( defined($+{rest}) ) {
            $tzrest = $+{rest};
        }

        return $tzprefix.$offset.$tzsuffix.$tzrest;
    }

    return $tzstr;
}

1;