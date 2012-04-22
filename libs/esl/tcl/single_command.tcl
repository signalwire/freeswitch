#!/usr/bin/tclsh
lappend auto_path .
package require esl

if { $argc < 1 } {
	puts "Usage:   tclsh $argv0 command arg(s)"
	puts "Example: tclsh $argv0 status"
	exit
}

#
# Open connection to FreeSWITCH ESL
#   (FreeSWITCH must be running for this to work)
#
ESLconnection esl {127.0.0.1} 8021 {ClueCon}

#
# Send request               (given as args to sendRecv)
# Get answer from FreeSWITCH (return value from sendRecv)
# Translate reponse to text  (getBody command)
# Print it                   (puts)
#
puts [ESLevent_getBody [esl sendRecv {api status}]]
