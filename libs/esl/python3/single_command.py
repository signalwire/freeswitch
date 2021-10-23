#!/usr/bin/env python

import string
import sys
from optparse import OptionParser
from ESL import *

def main(argv):
	
	try:
	   
		parser = OptionParser()
		parser.add_option("-a", "--auth", dest="auth", default="ClueCon",
								help="ESL password")
		parser.add_option("-s", "--server", dest="server", default="127.0.0.1",
								help="FreeSWITCH server IP address")
		parser.add_option("-p", "--port", dest="port", default="8021",
								help="FreeSWITCH server event socket port")
		parser.add_option("-c", "--command", dest="command",
								help="command to run, surround mutli word commands in \"\'s")

		(options, args) = parser.parse_args()

			
		con = ESLconnection(options.server, options.port, options.auth)
	#are we connected?

		if con.connected():
			#run command
			e = con.api(options.command)
			print e.getBody()

		else:

			print "Not Connected"
			sys.exit(2)

	except:
		
		print parser.get_usage()
		
if __name__ == "__main__":
    main(sys.argv[1:])