#!/usr/bin/env python

import string
import sys

from ESL import *

con = ESLconnection("localhost","8021","ClueCon")
#are we connected?

if con.connected:

  con.events("plain", "all");

  while 1:
  #my $e = $con->recvEventTimed(100);
    e = con.recvEvent()

    if e:
      print e.serialize()
