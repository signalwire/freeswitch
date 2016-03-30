#!/bin/bash

pcap=$1
shift
law=$1
shift

if [ -z $pcap ] ; then echo "usage $0 <pcap file> [<mu-law|a-law>]"; exit 255 ; fi

if [ -z $law ] ; then law="mu-law" ; fi

for ssrc in $(tshark -n -r $pcap -Y rtp -T fields -e rtp.ssrc -Eseparator=, | sort -u) ; do
    rm -f $pcap.$ssrc.raw $pcap.$ssrc.wav
    sudo tshark -n -r $pcap -Y "rtp && rtp.ssrc == $ssrc" -T fields -e rtp.payload | sed "s/:/ /g" | perl -ne 's/([0-9a-f]{2})/print chr hex $1/gie' >>  $pcap.$ssrc.raw
    sox -t raw -r 8000 -v 4 -c 1 -e $law $pcap.$ssrc.raw $pcap.$ssrc.wav
done


