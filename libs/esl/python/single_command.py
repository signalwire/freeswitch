#!/usr/bin/env python

import string
import sys
from ESL import *

con = ESLconnection("localhost","8021","ClueCon")
#are we connected?

if con.connected:

	#get argument passed to script
	command = string.join(sys.argv[1:])

	#run command
	e=con.sendRecv("api "+  command)
	print e.getBody()

else:

	print "Not Connected"

